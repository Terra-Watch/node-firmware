/**
 * imu_task.cpp
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * IMU Task Implementation — ADXL345 Accelerometer via I2C.
 *
 * DESIGN NOTES:
 * - Samples at 20 Hz (PERIOD_IMU_MS = 50 ms).
 *   Reasoning: Impact events in Phase 1 last 2–4 seconds (slam, collision).
 *   At 20 Hz we capture ≥40 samples per event — enough to detect spikes.
 *   This is much faster than GPS (1 Hz) as required by spec.
 *
 * - The ADXL345 is set to ±16g range. This lets us capture:
 *     • Normal rocking:  ≈ ±0.1–0.3g
 *     • Slam events:     3–5g peak (Z axis)
 *     • Collision:       4.5–7.5g peak (Y axis)
 *   (From Phase 1 dataset analysis)
 *
 * - Adafruit driver returns m/s². We divide by SENSORS_GRAVITY_STANDARD
 *   to get g units, matching Phase 1 columns.
 *
 * - FROZEN detection: ADXL345 stuck at same value for consecutive reads.
 * - ABNORMAL detection: |accel| > IMU_MAX_ACCEL_G (20g) OR values are NaN.
 *   Phase 1 shows sensor-fault ABNORMAL window outputs ±16g random values;
 *   this range check will flag it as invalid.
 *
 * - I2C bus is shared — MUST acquire xI2CMutex before any Wire call.
 * - If ADXL345 fails: accelValid = false, all other tasks continue.
 *
 * NOTE FOR PHASE 2: The high-frequency raw accel buffer here is the
 * input to the abnormal-motion/impact detection algorithm. That algorithm
 * will consume g_telemetry.accelX/Y/Z via the mutex. A ring buffer of
 * recent samples can be added to this task for more sophisticated detection.
 */

#include "imu_task.h"
#include "config.h"
#include "telemetry.h"

#include <Wire.h>
// Adafruit ADXL345 Unified Sensor library
// Install: Arduino Library Manager → "Adafruit ADXL345"
//          Also installs "Adafruit Unified Sensor" dependency
#include <Adafruit_ADXL345_U.h>
#include <math.h>

// ============================================================
// MODULE-PRIVATE STATE
// ============================================================

static Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(67890);

// Frozen-reading detection
#define IMU_FROZEN_THRESHOLD  10   // Flag FROZEN after this many identical reads (10 × 50ms = 500ms)
static float s_lastX = NAN;
static float s_lastY = NAN;
static float s_lastZ = NAN;
static uint8_t s_frozenCount = 0;

static bool s_initialized = false;

// ============================================================
// INITIALIZATION
// ============================================================

static bool imu_hw_init(void) {
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(MUTEX_I2C_TIMEOUT_MS)) != pdTRUE) {
        Serial.println("[IMU] ERROR: Could not acquire I2C mutex for init");
        return false;
    }

    bool ok = accel.begin();

    if (ok) {
        // Set range to ±16g to capture impact events (slams up to 7.5g peak)
        accel.setRange(ADXL345_RANGE_16_G);
    }

    xSemaphoreGive(xI2CMutex);

    if (!ok) {
        Serial.println("[IMU] ERROR: ADXL345 not detected at 0x53. Check wiring.");
        return false;
    }

    Serial.println("[IMU] ADXL345 initialized OK at I2C 0x53, range ±16g");
    return true;
}

// ============================================================
// FROZEN DETECTION
// ============================================================

static bool detect_frozen(float x, float y, float z) {
    const float EPSILON = 0.001f;
    if (fabsf(x - s_lastX) < EPSILON &&
        fabsf(y - s_lastY) < EPSILON &&
        fabsf(z - s_lastZ) < EPSILON) {
        s_frozenCount++;
    } else {
        s_frozenCount = 0;
        s_lastX = x; s_lastY = y; s_lastZ = z;
    }
    return (s_frozenCount >= IMU_FROZEN_THRESHOLD);
}

// ============================================================
// VALUE VALIDATION
// ============================================================

static bool imu_values_valid(float x, float y, float z) {
    if (isnan(x) || isnan(y) || isnan(z)) return false;
    if (isinf(x) || isinf(y) || isinf(z)) return false;
    // Check for out-of-range (ABNORMAL sensor fault produces ±16g random garbage)
    if (fabsf(x) > IMU_MAX_ACCEL_G) return false;
    if (fabsf(y) > IMU_MAX_ACCEL_G) return false;
    if (fabsf(z) > IMU_MAX_ACCEL_G) return false;
    return true;
}

// ============================================================
// IMU TASK FUNCTION
// ============================================================

static void imu_task_fn(void *pvParameters) {
    (void)pvParameters;

    Serial.printf("[IMU] Task started on Core %d. Period: %d ms (%.0f Hz)\n",
                  xPortGetCoreID(), PERIOD_IMU_MS, 1000.0f / PERIOD_IMU_MS);

    // Attempt hardware init with retries
    uint8_t retries = 3;
    while (retries > 0 && !s_initialized) {
        vTaskDelay(pdMS_TO_TICKS(200));
        s_initialized = imu_hw_init();
        retries--;
    }

    if (!s_initialized) {
        Serial.println("[IMU] FATAL: Could not initialize ADXL345 after retries.");
        HealthEvent evt = { SENSOR_IMU, HEALTH_EVENT_FAULT };
        xQueueSend(xHealthQueue, &evt, 0);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        bool readOk  = false;
        float gx = 0.0f, gy = 0.0f, gz = 0.0f;

        if (s_initialized) {
            sensors_event_t event;

            if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(MUTEX_I2C_TIMEOUT_MS)) == pdTRUE) {
                bool gotEvent = accel.getEvent(&event);
                xSemaphoreGive(xI2CMutex);

                if (gotEvent) {
                    // Convert m/s² → g
                    float raw_x = event.acceleration.x / SENSORS_GRAVITY_STANDARD;
                    float raw_y = event.acceleration.y / SENSORS_GRAVITY_STANDARD;
                    float raw_z = event.acceleration.z / SENSORS_GRAVITY_STANDARD;

                    if (imu_values_valid(raw_x, raw_y, raw_z)) {
                        bool frozen = detect_frozen(raw_x, raw_y, raw_z);
                        if (!frozen) {
                            gx = raw_x;
                            gy = raw_y;
                            gz = raw_z;
                            readOk = true;
                        } else {
                            Serial.println("[IMU] WARNING: FROZEN reading detected");
                            HealthEvent evt = { SENSOR_IMU, HEALTH_EVENT_FAULT };
                            xQueueSend(xHealthQueue, &evt, 0);
                        }
                    } else {
                        // ABNORMAL: out-of-range values — sensor fault
                        HealthEvent evt = { SENSOR_IMU, HEALTH_EVENT_FAULT };
                        xQueueSend(xHealthQueue, &evt, 0);
                    }
                }
            } else {
                Serial.println("[IMU] WARNING: Could not acquire I2C mutex");
            }
        }

        // -- Update telemetry --
        if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) == pdTRUE) {
            static bool wasFaulty = true;

            if (readOk) {
                g_telemetry.accelX    = gx;
                g_telemetry.accelY    = gy;
                g_telemetry.accelZ    = gz;
                g_telemetry.accelValid = true;

                if (wasFaulty) {
                    HealthEvent evt = { SENSOR_IMU, HEALTH_EVENT_RECOVERED };
                    xQueueSend(xHealthQueue, &evt, 0);
                    wasFaulty = false;
                }
            } else {
                g_telemetry.accelValid = false;
                wasFaulty = true;
            }

            telemetry_update_rollup();
            g_telemetry.imuLastRun = millis();

            xSemaphoreGive(xTelemetryMutex);
        } else {
            Serial.println("[IMU] WARNING: Could not acquire telemetry mutex");
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_IMU_MS));
    }
    vTaskDelete(NULL);
}

// ============================================================
// PUBLIC API
// ============================================================

void imu_task_start(void) {
    BaseType_t result = xTaskCreatePinnedToCore(
        imu_task_fn,
        "IMU_Task",
        STACK_IMU,
        NULL,
        PRIORITY_IMU,
        NULL,
        CORE_IMU
    );

    if (result != pdPASS) {
        Serial.println("[IMU] FATAL: Failed to create IMU task!");
    }
}
