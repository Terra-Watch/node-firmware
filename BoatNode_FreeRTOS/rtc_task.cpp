/**
 * rtc_task.cpp
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * RTC Task Implementation — DS3231 via I2C.
 *
 * DESIGN NOTES:
 * - The DS3231 shares the I2C bus with HMC5883L and ADXL345.
 *   xI2CMutex is acquired for every I2C transaction.
 * - The DS3231 is treated as the primary time source. The Phase 1
 *   dataset shows rtc_status = OK for all 5000 records — the DS3231
 *   is the one sensor expected to never fail in normal operation.
 * - If the DS3231 fails:
 *     → rtcValid = false
 *     → timestamp falls back to millis()-based uptime seconds
 *     → a HEALTH_EVENT_FAULT is posted
 *     → all other tasks continue
 * - Year validation: if year < RTC_MIN_VALID_YEAR (2024), the DS3231
 *   may not be set (fresh chip default is 2000). We mark rtcValid = false
 *   and report the issue via Serial so the user knows to set the time.
 *   (Time setting can be done via a one-time setup or NTP-GPS sync in Phase 2.)
 */

#include "rtc_task.h"
#include "config.h"
#include "telemetry.h"

#include <Wire.h>
// Adafruit RTClib
// Install: Arduino Library Manager → "RTClib"
#include <RTClib.h>
#include <stdio.h>

// ============================================================
// MODULE-PRIVATE STATE
// ============================================================

static RTC_DS3231 rtc;
static bool s_initialized = false;

// Fallback: if RTC fails, we use millis() as an uptime-based timestamp.
// This is NOT a wall-clock time, but it keeps the timestamp field meaningful.
static uint32_t s_fallbackStartSeconds = 0;  // Set once when RTC fails

// ============================================================
// INITIALIZATION
// ============================================================

static bool rtc_hw_init(void) {
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(MUTEX_I2C_TIMEOUT_MS)) != pdTRUE) {
        Serial.println("[RTC] ERROR: Could not acquire I2C mutex for init");
        return false;
    }

    bool ok = rtc.begin();

    xSemaphoreGive(xI2CMutex);

    if (!ok) {
        Serial.println("[RTC] ERROR: DS3231 not detected at 0x68. Check wiring.");
        return false;
    }

    // Check if the DS3231 lost power (might need time to be set)
    if (rtc.lostPower()) {
        Serial.println("[RTC] WARNING: DS3231 lost power — time may not be set.");
        // In production, GPS PPS or NTP would set it. For now, log the warning.
        // We still allow rtcValid if the year is reasonable.
    }

    Serial.println("[RTC] DS3231 initialized OK at I2C 0x68");
    return true;
}

// ============================================================
// TIMESTAMP FORMAT HELPER
// Fills a buffer with "YYYY-MM-DD HH:MM:SS" (19 chars + null = 20)
// ============================================================

static void format_datetime(const DateTime &dt, char *buf, size_t bufLen) {
    snprintf(buf, bufLen, "%04d-%02d-%02d %02d:%02d:%02d",
             dt.year(), dt.month(), dt.day(),
             dt.hour(), dt.minute(), dt.second());
}

// ============================================================
// RTC TASK FUNCTION
// ============================================================

static void rtc_task_fn(void *pvParameters) {
    (void)pvParameters;

    Serial.printf("[RTC] Task started on Core %d\n", xPortGetCoreID());

    // Attempt hardware init with retries
    uint8_t retries = 3;
    while (retries > 0 && !s_initialized) {
        vTaskDelay(pdMS_TO_TICKS(200));
        s_initialized = rtc_hw_init();
        retries--;
    }

    if (!s_initialized) {
        Serial.println("[RTC] FATAL: Could not initialize DS3231 after retries.");
        // Set fallback start point
        s_fallbackStartSeconds = (uint32_t)(millis() / 1000UL);
        HealthEvent evt = { SENSOR_RTC, HEALTH_EVENT_FAULT };
        xQueueSend(xHealthQueue, &evt, 0);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        bool readOk = false;
        uint32_t ts  = 0;
        char tsBuf[20] = "0000-00-00 00:00:00";

        if (s_initialized) {
            DateTime now;

            if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(MUTEX_I2C_TIMEOUT_MS)) == pdTRUE) {
                now = rtc.now();
                xSemaphoreGive(xI2CMutex);

                // Validate year
                if (now.year() >= RTC_MIN_VALID_YEAR) {
                    ts = now.unixtime();    // Seconds since 1970-01-01 UTC
                    format_datetime(now, tsBuf, sizeof(tsBuf));
                    readOk = true;
                } else {
                    Serial.printf("[RTC] WARNING: Year %d is before %d — DS3231 may not be set.\n",
                                  now.year(), RTC_MIN_VALID_YEAR);
                }
            } else {
                Serial.println("[RTC] WARNING: Could not acquire I2C mutex");
            }
        }

        // -- Fallback: uptime-based timestamp if RTC not available --
        if (!readOk) {
            uint32_t uptimeSec = (uint32_t)(millis() / 1000UL);
            ts = s_fallbackStartSeconds + uptimeSec;
            snprintf(tsBuf, sizeof(tsBuf), "UPTIME+%us", uptimeSec);
        }

        // -- Update telemetry --
        if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) == pdTRUE) {
            static bool wasFaulty = true;

            g_telemetry.timestamp = ts;
            strncpy(g_telemetry.timeString, tsBuf, sizeof(g_telemetry.timeString));
            g_telemetry.timeString[sizeof(g_telemetry.timeString) - 1] = '\0';

            if (readOk) {
                g_telemetry.rtcValid = true;
                if (wasFaulty) {
                    HealthEvent evt = { SENSOR_RTC, HEALTH_EVENT_RECOVERED };
                    xQueueSend(xHealthQueue, &evt, 0);
                    wasFaulty = false;
                }
            } else {
                g_telemetry.rtcValid = false;
                wasFaulty = true;
            }

            telemetry_update_rollup();
            g_telemetry.rtcLastRun = millis();

            xSemaphoreGive(xTelemetryMutex);
        } else {
            Serial.println("[RTC] WARNING: Could not acquire telemetry mutex");
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_RTC_MS));
    }
    vTaskDelete(NULL);
}

// ============================================================
// PUBLIC API
// ============================================================

void rtc_task_start(void) {
    BaseType_t result = xTaskCreatePinnedToCore(
        rtc_task_fn,
        "RTC_Task",
        STACK_RTC,
        NULL,
        PRIORITY_RTC,
        NULL,
        CORE_RTC
    );

    if (result != pdPASS) {
        Serial.println("[RTC] FATAL: Failed to create RTC task!");
    }
}
