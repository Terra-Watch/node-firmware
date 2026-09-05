/**
 * telemetry_task.cpp
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * Telemetry Task Implementation — Serial Monitor debug output.
 *
 * DESIGN NOTES:
 * - Reads a SNAPSHOT of g_telemetry under mutex, then prints without
 *   holding the mutex. This ensures sensor tasks are never blocked
 *   waiting for Serial to finish printing.
 * - Prints at PERIOD_TELEMETRY_MS (1000 ms = 1 Hz) — readable on
 *   Arduino IDE Serial Monitor.
 * - Shows heap and GPS age info for hardware diagnostics.
 */

#include "telemetry_task.h"
#include "config.h"
#include "telemetry.h"
#include <math.h>

// ============================================================
// SNAPSHOT (local copy taken under mutex)
// ============================================================

// We copy the entire struct so we can print outside the mutex window.
static BoatTelemetry s_snapshot;

// ============================================================
// HELPER: health state to string
// ============================================================

static const char* health_state_str(SystemHealthState s) {
    switch (s) {
        case HEALTH_HEALTHY:  return "HEALTHY";
        case HEALTH_DEGRADED: return "DEGRADED";
        case HEALTH_CRITICAL: return "CRITICAL";
        default:              return "UNKNOWN";
    }
}

// ============================================================
// PRINT FUNCTION (called outside mutex)
// ============================================================

static void print_telemetry(void) {
    Serial.println();
    Serial.println(DEBUG_DIVIDER);
    Serial.println("     PALAVARIRAKSHA — BOATNODE");
    Serial.println(DEBUG_DIVIDER);

    // Time
    Serial.printf("Time : %s\n", s_snapshot.timeString);

    // GPS
    Serial.println("\n[GPS]");
    if (s_snapshot.gpsValid) {
        Serial.printf("  Fix        : YES (%d satellites, HDOP %.1f)\n",
                      s_snapshot.satellites, s_snapshot.hdop);
        Serial.printf("  Latitude   : %.6f\n",  s_snapshot.latitude);
        Serial.printf("  Longitude  : %.6f\n",  s_snapshot.longitude);
        Serial.printf("  Speed      : %.2f knots\n", s_snapshot.gpsSpeed);
        Serial.printf("  Course     : %.1f deg\n",   s_snapshot.gpsCourse);
        Serial.printf("  Last update: %lu ms ago\n",
                      (unsigned long)(millis() - s_snapshot.lastGpsUpdateMs));
    } else {
        Serial.println("  Fix        : NO (no fix or stale)");
        if (s_snapshot.lastGpsUpdateMs > 0) {
            Serial.printf("  Last fix   : %lu ms ago\n",
                          (unsigned long)(millis() - s_snapshot.lastGpsUpdateMs));
        } else {
            Serial.println("  Last fix   : Never");
        }
    }

    // Compass
    Serial.println("\n[COMPASS]");
    if (s_snapshot.compassValid) {
        Serial.printf("  Heading    : %.1f deg\n",    s_snapshot.compassHeading);
        Serial.printf("  Magnetic   : X=%.3f Y=%.3f Z=%.3f (gauss)\n",
                      s_snapshot.magneticX, s_snapshot.magneticY, s_snapshot.magneticZ);
    } else {
        Serial.println("  Status     : UNAVAILABLE");
    }

    // IMU
    Serial.println("\n[IMU - ADXL345]");
    if (s_snapshot.accelValid) {
        Serial.printf("  Accel X    : %+.3f g\n", s_snapshot.accelX);
        Serial.printf("  Accel Y    : %+.3f g\n", s_snapshot.accelY);
        Serial.printf("  Accel Z    : %+.3f g  (expect ~1.0g at rest)\n", s_snapshot.accelZ);
    } else {
        Serial.println("  Status     : UNAVAILABLE");
    }

    // RTC
    Serial.println("\n[RTC - DS3231]");
    Serial.printf("  Valid      : %s\n", s_snapshot.rtcValid ? "YES" : "NO");

    // Local safety analysis
    Serial.println("\n[LOCAL ANALYSIS]");
    Serial.printf("  GPS noise : %s (score %.1f)\n",
                  s_snapshot.gpsNoiseDetected ? "YES" : "NO",
                  s_snapshot.gpsNoiseScore);
    Serial.printf("  GPS loss  : %s\n", s_snapshot.gpsLossDetected ? "YES" : "NO");
    Serial.printf("  DR active : %s\n", s_snapshot.deadReckoningActive ? "YES" : "NO");
    Serial.printf("  IMBL warn : %s | dist %.1f m\n",
                  s_snapshot.imblWarning ? "YES" : "NO",
                  s_snapshot.distanceToBoundaryM);
    Serial.printf("  Impact    : %s | peak %.2f g\n",
                  s_snapshot.impactDetected ? "YES" : "NO",
                  s_snapshot.impactPeakG);
    Serial.printf("  Fusion    : %s | quality %.1f/100\n",
                  s_snapshot.fusionHealthy ? "HEALTHY" : "LOW",
                  s_snapshot.fusedQualityScore);

    uint8_t packetBuffer[JALARI_TELEMETRY_PAYLOAD_SIZE] = {0};
    size_t packetLength = 0;
    if (telemetry_pack_jalari_payload(packetBuffer, sizeof(packetBuffer), s_snapshot, packetLength)) {
        Serial.printf("  JALARI payload: %u bytes [v=%u flags=0x%02X q=%u]\n",
                      (unsigned int)packetLength,
                      packetBuffer[0],
                      packetBuffer[1],
                      (unsigned int)((packetBuffer[26] | (packetBuffer[27] << 8)) & 0xFFFFU));
    }

    // System Health
    Serial.println("\n[SYSTEM HEALTH]");
    Serial.printf("  GPS        : %s\n", s_snapshot.gpsValid     ? "OK" : "FAULT");
    Serial.printf("  Compass    : %s\n", s_snapshot.compassValid  ? "OK" : "FAULT");
    Serial.printf("  IMU        : %s\n", s_snapshot.accelValid    ? "OK" : "FAULT");
    Serial.printf("  RTC        : %s\n", s_snapshot.rtcValid      ? "OK" : "FAULT");
    Serial.printf("  Overall    : %s\n", health_state_str(s_snapshot.healthState));

    // Memory diagnostics
    Serial.println("\n[MEMORY]");
    Serial.printf("  Free heap  : %lu bytes\n",   (unsigned long)ESP.getFreeHeap());
    Serial.printf("  Min heap   : %lu bytes\n",   (unsigned long)ESP.getMinFreeHeap());

    // Task heartbeat age (diagnostic)
    uint32_t nowMs = millis();
    Serial.println("\n[HEARTBEATS (ms since last run)]");
    Serial.printf("  GPS        : %lu\n", (unsigned long)(nowMs - s_snapshot.gpsLastRun));
    Serial.printf("  Compass    : %lu\n", (unsigned long)(nowMs - s_snapshot.compassLastRun));
    Serial.printf("  IMU        : %lu\n", (unsigned long)(nowMs - s_snapshot.imuLastRun));
    Serial.printf("  RTC        : %lu\n", (unsigned long)(nowMs - s_snapshot.rtcLastRun));

    Serial.println(DEBUG_DIVIDER);
}

// ============================================================
// TELEMETRY TASK FUNCTION
// ============================================================

static void telemetry_task_fn(void *pvParameters) {
    (void)pvParameters;

    Serial.printf("[TELEMETRY] Task started on Core %d\n", xPortGetCoreID());

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // -- Take snapshot under mutex (fast: just memcpy) --
        if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) == pdTRUE) {
            memcpy(&s_snapshot, &g_telemetry, sizeof(BoatTelemetry));

            // Update telemetry task heartbeat
            g_telemetry.telemetryLastRun = millis();

            xSemaphoreGive(xTelemetryMutex);

            // -- Print outside mutex so sensor tasks aren't blocked --
            print_telemetry();
        } else {
            Serial.println("[TELEMETRY] WARNING: Could not acquire telemetry mutex");
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_TELEMETRY_MS));
    }
    vTaskDelete(NULL);
}

// ============================================================
// PUBLIC API
// ============================================================

void telemetry_task_start(void) {
    BaseType_t result = xTaskCreatePinnedToCore(
        telemetry_task_fn,
        "Telemetry_Task",
        STACK_TELEMETRY,
        NULL,
        PRIORITY_TELEMETRY,
        NULL,
        CORE_TELEMETRY
    );

    if (result != pdPASS) {
        Serial.println("[TELEMETRY] FATAL: Failed to create Telemetry task!");
    }
}
