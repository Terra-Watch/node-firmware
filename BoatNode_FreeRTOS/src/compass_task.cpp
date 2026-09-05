/**
 * compass_task.cpp
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * Compass Task Implementation — HMC5883L via I2C.
 *
 * DESIGN NOTES:
 * - The HMC5883L shares the I2C bus with ADXL345 (0x53) and DS3231 (0x68).
 *   Every I2C access MUST acquire xI2CMutex before calling Wire or the sensor
 *   driver, and release it immediately after the read is complete.
 * - Heading is computed from raw X/Y magnetic field vectors using atan2().
 *   The result is normalized to [0, 360] degrees.
 * - If the HMC5883L fails to initialize or returns an error:
 *     → compassValid = false
 *     → heading remains at its last known good value
 *     → a HEALTH_EVENT_FAULT is posted to xHealthQueue
 *     → all other tasks continue running normally
 * - FROZEN detection (Phase 1 COMPASS_FROZEN scenario):
 *   We track the last reading and flag FROZEN if X/Y/Z do not change
 *   for COMPASS_FROZEN_THRESHOLD consecutive reads.
 */

#include "compass_task.h"
#include "config.h"
#include "telemetry.h"

#include <Wire.h>
// Adafruit HMC5883L Unified Sensor library
// Install: Arduino Library Manager → "Adafruit HMC5883 Unified"
//          Also installs "Adafruit Unified Sensor" dependency
#include <Adafruit_HMC5883_U.h>
#include <math.h>

// ============================================================
// MODULE-PRIVATE STATE
// ============================================================

// Sensor ID can be any unique number — used by Adafruit Unified sensor
static Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

// Frozen-reading detection
#define COMPASS_FROZEN_THRESHOLD   5   // Flag FROZEN after this many identical reads

static float s_lastX = NAN;
static float s_lastY = NAN;
static float s_lastZ = NAN;
static uint8_t s_frozenCount = 0;

static bool s_initialized = false;

// ============================================================
// INITIALIZATION (called once inside task, after scheduler runs)
// ============================================================

static bool compass_hw_init(void) {
    // Acquire I2C mutex for initialization
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(MUTEX_I2C_TIMEOUT_MS)) != pdTRUE) {
        Serial.println("[COMPASS] ERROR: Could not acquire I2C mutex for init");
        return false;
    }

    bool ok = mag.begin();

    xSemaphoreGive(xI2CMutex);

    if (!ok) {
        Serial.println("[COMPASS] ERROR: HMC5883L not detected at 0x1E. Check wiring.");
        return false;
    }

    Serial.println("[COMPASS] HMC5883L initialized OK at I2C 0x1E");
    return true;
}

// ============================================================
// HEADING CALCULATION
// ============================================================

/**
 * @brief Convert raw magnetic X/Y to heading in degrees [0, 360).
 *
 * Standard formula for HMC5883L with sensor lying flat:
 *   heading = atan2(Y, X) * (180 / PI)
 *   normalize: if < 0, add 360
 *
 * Declination correction can be added here in Phase 2 once the
 * actual boat's operating location (Chennai coast) declination is known.
 * Chennai magnetic declination ≈ 0.7° West (approximately 0, so negligible
 * for Phase 1 prototype, but should be refined in Phase 2).
 */
static float compute_heading(float x, float y) {
    float heading = atan2(y, x) * (180.0f / (float)M_PI);

    // Normalize to [0, 360)
    if (heading < 0.0f) {
        heading += 360.0f;
    }

    // Safety clamp
    if (heading >= 360.0f) heading -= 360.0f;

    return heading;
}

// ============================================================
// FROZEN DETECTION
// ============================================================

static bool detect_frozen(float x, float y, float z) {
    // If this reading is identical (within float epsilon) to the last, increment counter
    const float EPSILON = 0.001f;
    if (fabsf(x - s_lastX) < EPSILON &&
        fabsf(y - s_lastY) < EPSILON &&
        fabsf(z - s_lastZ) < EPSILON) {
        s_frozenCount++;
    } else {
        s_frozenCount = 0;
        s_lastX = x;
        s_lastY = y;
        s_lastZ = z;
    }
    return (s_frozenCount >= COMPASS_FROZEN_THRESHOLD);
}

// ============================================================
// COMPASS TASK FUNCTION
// ============================================================

static void compass_task_fn(void *pvParameters) {
    (void)pvParameters;

    Serial.printf("[COMPASS] Task started on Core %d\n", xPortGetCoreID());

    // Attempt hardware init (retry a few times if bus is busy at startup)
    uint8_t retries = 3;
    while (retries > 0 && !s_initialized) {
        vTaskDelay(pdMS_TO_TICKS(200));   // Short delay for I2C to settle
        s_initialized = compass_hw_init();
        retries--;
    }

    if (!s_initialized) {
        Serial.println("[COMPASS] FATAL: Could not initialize HMC5883L after retries.");
        // Post fault event and keep running — do NOT crash the system
        HealthEvent evt = { SENSOR_COMPASS, HEALTH_EVENT_FAULT };
        xQueueSend(xHealthQueue, &evt, 0);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        bool readOk = false;
        float heading = 0.0f;
        float mx = 0.0f, my = 0.0f, mz = 0.0f;

        if (s_initialized) {
            sensors_event_t event;

            // Acquire I2C mutex before reading sensor
            if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(MUTEX_I2C_TIMEOUT_MS)) == pdTRUE) {
                bool gotEvent = mag.getEvent(&event);
                xSemaphoreGive(xI2CMutex);

                if (gotEvent) {
                    mx = event.magnetic.x;
                    my = event.magnetic.y;
                    mz = event.magnetic.z;

                    // Check for NaN (Adafruit driver may return NaN on I2C error)
                    if (!isnan(mx) && !isnan(my) && !isnan(mz)) {
                        bool frozen = detect_frozen(mx, my, mz);

                        if (!frozen) {
                            heading = compute_heading(mx, my);
                            readOk  = true;
                        } else {
                            Serial.println("[COMPASS] WARNING: FROZEN reading detected");
                            HealthEvent evt = { SENSOR_COMPASS, HEALTH_EVENT_FAULT };
                            xQueueSend(xHealthQueue, &evt, 0);
                        }
                    }
                }
            } else {
                Serial.println("[COMPASS] WARNING: Could not acquire I2C mutex");
            }
        }

        // -- Update telemetry --
        if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) == pdTRUE) {
            static bool wasFaulty = true;

            if (readOk) {
                g_telemetry.compassHeading = heading;
                g_telemetry.magneticX      = mx;
                g_telemetry.magneticY      = my;
                g_telemetry.magneticZ      = mz;
                g_telemetry.compassValid   = true;

                if (wasFaulty) {
                    HealthEvent evt = { SENSOR_COMPASS, HEALTH_EVENT_RECOVERED };
                    xQueueSend(xHealthQueue, &evt, 0);
                    wasFaulty = false;
                }
            } else {
                g_telemetry.compassValid = false;
                wasFaulty = true;
            }

            telemetry_update_rollup();
            g_telemetry.compassLastRun = millis();

            xSemaphoreGive(xTelemetryMutex);
        } else {
            Serial.println("[COMPASS] WARNING: Could not acquire telemetry mutex");
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_COMPASS_MS));
    }
    vTaskDelete(NULL);
}

// ============================================================
// PUBLIC API
// ============================================================

void compass_task_start(void) {
    BaseType_t result = xTaskCreatePinnedToCore(
        compass_task_fn,
        "Compass_Task",
        STACK_COMPASS,
        NULL,
        PRIORITY_COMPASS,
        NULL,
        CORE_COMPASS
    );

    if (result != pdPASS) {
        Serial.println("[COMPASS] FATAL: Failed to create Compass task!");
    }
}
