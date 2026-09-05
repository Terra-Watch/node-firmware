/**
 * gps_task.cpp
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * GPS Task Implementation.
 *
 * DESIGN NOTES:
 * - Uses UART2 (HardwareSerial Serial2) on GPIO16(RX)/GPIO17(TX)
 * - TinyGPSPlus is fed one byte at a time from the UART buffer.
 *   This is non-blocking: we read whatever bytes are available each
 *   cycle, then delay to the next period using vTaskDelayUntil().
 * - GPS data arrives at ~1 Hz from the NEO-6M in NMEA sentences.
 *   We poll UART every 1000 ms. Between periods we do not busy-wait,
 *   so other tasks get CPU time freely.
 * - NMEA sentences can contain $GPRMC (has speed/course) and $GPGGA
 *   (has lat/lon/satellites/HDOP). TinyGPSPlus merges both.
 * - If the GPS module is disconnected or loses fix:
 *     → gpsValid = false
 *     → latitude/longitude = NAN
 *     → other tasks continue unaffected
 */

#include "gps_task.h"
#include "config.h"
#include "telemetry.h"

// TinyGPSPlus — install via Arduino Library Manager
// or: https://github.com/mikalhart/TinyGPSPlus
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ============================================================
// MODULE-PRIVATE STATE
// ============================================================

static TinyGPSPlus gps;
static HardwareSerial gpsSerial(GPS_UART_PORT);  // UART2

// ============================================================
// VALIDATION HELPER
// ============================================================

/**
 * @brief Validates a TinyGPSPlus-decoded fix against known-good ranges.
 *
 * Returns true only if:
 *  - A valid NMEA fix is present (isValid())
 *  - Latitude is within [-90, 90]
 *  - Longitude is within [-180, 180]
 *  - Speed is non-negative and below GPS_MAX_SPEED_KNOTS
 *  - Course is 0–360 degrees
 *  - At least GPS_MIN_SATELLITES satellites are in use
 *
 * The NEO-6M can return a "fix" with 0 satellites in some edge cases
 * (FAULT_STALE scenario from Phase 1). The satellite count check catches this.
 */
static bool gps_validate(void) {
    if (!gps.location.isValid()) return false;

    double lat = gps.location.lat();
    double lon = gps.location.lng();

    if (lat < -90.0 || lat > 90.0)   return false;
    if (lon < -180.0 || lon > 180.0) return false;

    if (gps.speed.isValid()) {
        float spd = (float)gps.speed.knots();
        if (spd < 0.0f || spd > GPS_MAX_SPEED_KNOTS) return false;
    }

    if (gps.course.isValid()) {
        float crs = (float)gps.course.deg();
        if (crs < 0.0f || crs > 360.0f) return false;
    }

    if (gps.satellites.isValid()) {
        if (gps.satellites.value() < GPS_MIN_SATELLITES) return false;
    }

    return true;
}

// ============================================================
// GPS TASK FUNCTION
// ============================================================

static void gps_task_fn(void *pvParameters) {
    (void)pvParameters;

    // -- Initialize UART2 for NEO-6M --
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.printf("[GPS] Task started on Core %d. UART2 RX=%d TX=%d @ %d baud\n",
                  xPortGetCoreID(), GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // -- Feed all available UART bytes to TinyGPSPlus --
        // This loop is very fast: typically < 1 ms for a full NMEA sentence.
        while (gpsSerial.available() > 0) {
            gps.encode(gpsSerial.read());
        }

        // -- Check whether we have fresh, valid data --
        bool isValid = gps_validate();

        uint32_t nowMs = millis();

        // -- Detect staleness regardless of current fix status --
        // If we had a valid fix before but haven't updated in GPS_STALE_TIMEOUT_MS,
        // mark gpsValid = false. This catches FAULT_STALE from Phase 1 dataset.
        bool stale = false;
        if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) == pdTRUE) {
            if (g_telemetry.lastGpsUpdateMs != 0) {
                stale = (nowMs - g_telemetry.lastGpsUpdateMs) > GPS_STALE_TIMEOUT_MS;
            }
            xSemaphoreGive(xTelemetryMutex);
        }

        // -- Update telemetry --
        if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) == pdTRUE) {

            if (isValid && !stale) {
                // Good fix — populate all GPS fields
                g_telemetry.latitude        = gps.location.lat();
                g_telemetry.longitude       = gps.location.lng();
                g_telemetry.gpsSpeed        = gps.speed.isValid()     ? (float)gps.speed.knots() : 0.0f;
                g_telemetry.gpsCourse       = gps.course.isValid()    ? (float)gps.course.deg()  : 0.0f;
                g_telemetry.gpsFix          = true;
                g_telemetry.satellites      = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
                g_telemetry.hdop            = gps.hdop.isValid()       ? (float)gps.hdop.hdop()           : 99.9f;
                g_telemetry.lastGpsUpdateMs = nowMs;
                g_telemetry.gpsValid        = true;

                // Post recovery event if we were previously invalid
                static bool wasFaulty = true;
                if (wasFaulty) {
                    HealthEvent evt = { SENSOR_GPS, HEALTH_EVENT_RECOVERED };
                    xQueueSend(xHealthQueue, &evt, 0);
                    wasFaulty = false;
                }
            } else {
                // No fix, invalid data, or stale — keep last known lat/lon as NAN
                // so downstream code can detect the gap
                g_telemetry.latitude   = NAN;
                g_telemetry.longitude  = NAN;
                g_telemetry.gpsFix     = false;
                g_telemetry.gpsValid   = false;

                if (stale) {
                    HealthEvent evt = { SENSOR_GPS, HEALTH_EVENT_STALE };
                    xQueueSend(xHealthQueue, &evt, 0);
                } else if (gps.failedChecksum() > 0) {
                    HealthEvent evt = { SENSOR_GPS, HEALTH_EVENT_FAULT };
                    xQueueSend(xHealthQueue, &evt, 0);
                }

                static bool wasFaulty = false;
                wasFaulty = true;
                (void)wasFaulty;  // suppress warning — used in recovery path above
            }

            // Update telemetry rollup and heartbeat
            telemetry_update_rollup();
            g_telemetry.gpsLastRun = nowMs;

            xSemaphoreGive(xTelemetryMutex);
        } else {
            // Could not acquire mutex — log and continue
            Serial.println("[GPS] WARNING: Could not acquire telemetry mutex");
        }

        // -- Periodic delay (accurate timing using vTaskDelayUntil) --
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_GPS_MS));
    }
    // Tasks must never return; if they do, delete self.
    vTaskDelete(NULL);
}

// ============================================================
// PUBLIC API
// ============================================================

void gps_task_start(void) {
    BaseType_t result = xTaskCreatePinnedToCore(
        gps_task_fn,
        "GPS_Task",
        STACK_GPS,
        NULL,
        PRIORITY_GPS,
        NULL,
        CORE_GPS
    );

    if (result != pdPASS) {
        Serial.println("[GPS] FATAL: Failed to create GPS task!");
    }
}
