/**
 * system_health.cpp
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * System Health Task Implementation.
 *
 * RESPONSIBILITIES:
 *  1. Drain xHealthQueue — process fault/recovered/stale events from sensors.
 *  2. Check task heartbeats — if any task hasn't updated in HEARTBEAT_TIMEOUT_MS,
 *     it's considered hung and the system is flagged CRITICAL.
 *  3. Check sensor validity flags directly from g_telemetry.
 *  4. Classify system state:
 *       HEALTHY  = all sensors valid, all tasks heartbeating
 *       DEGRADED = 1-2 sensors invalid, tasks running
 *       CRITICAL = multiple sensors down, OR a task has stopped running
 *  5. Write final healthState to g_telemetry.healthState under mutex.
 *  6. Report ESP32 task stack high-water marks to Serial periodically.
 *
 * ESP32 TASK WATCHDOG (TWDT) NOTE:
 * The ESP32 Arduino environment includes a built-in Task Watchdog Timer (TWDT).
 * By default, the IDLE tasks on both cores subscribe to the TWDT.
 * If an application task starves the IDLE task (e.g., by looping at very
 * high priority without yielding), the TWDT resets the ESP32.
 *
 * For this firmware:
 * - All tasks use vTaskDelayUntil() which YIELDS to the scheduler each period.
 * - The IDLE task therefore runs regularly on both cores.
 * - This means the default TWDT behavior already provides a system-level watchdog.
 *
 * For explicit TWDT subscription (advanced):
 *   #include "esp_task_wdt.h"
 *   esp_task_wdt_add(NULL);          // Add current task to TWDT
 *   esp_task_wdt_reset();            // Reset watchdog (call periodically)
 *   esp_task_wdt_delete(NULL);       // Remove task from TWDT
 *
 * We do NOT add it here to keep the implementation straightforward, but the
 * heartbeat mechanism below achieves the same intent at the application level.
 */

#include "system_health.h"
#include "config.h"
#include "telemetry.h"

// ============================================================
// MODULE STATE
// ============================================================

// Track how many sensors are currently in fault state
static uint8_t s_faultCount = 0;

// Track individual sensor fault flags (true = currently in fault)
static bool s_gpsInFault     = false;
static bool s_compassInFault = false;
static bool s_imuInFault     = false;
static bool s_rtcInFault     = false;

// Stack high-water mark report interval
#define STACK_REPORT_INTERVAL_CYCLES   15   // Report every 15 health cycles (~30 s)
static uint16_t s_cycleCount = 0;

// Task handles for stack monitoring (set in main sketch, passed via parameter or global)
// For simplicity, we use UBaseType_t uxTaskGetStackHighWaterMark() on NULL (current task)
// and report free heap as a proxy for system memory health.

// ============================================================
// PROCESS HEALTH QUEUE
// ============================================================

static void process_health_queue(void) {
    HealthEvent evt;
    // Drain all pending events (non-blocking: timeout = 0)
    while (xQueueReceive(xHealthQueue, &evt, 0) == pdTRUE) {
        const char* sensorName = "";
        bool* faultFlag = nullptr;

        switch (evt.sensor) {
            case SENSOR_GPS:
                sensorName = "GPS";
                faultFlag  = &s_gpsInFault;
                break;
            case SENSOR_COMPASS:
                sensorName = "COMPASS";
                faultFlag  = &s_compassInFault;
                break;
            case SENSOR_IMU:
                sensorName = "IMU";
                faultFlag  = &s_imuInFault;
                break;
            case SENSOR_RTC:
                sensorName = "RTC";
                faultFlag  = &s_rtcInFault;
                break;
        }

        if (faultFlag == nullptr) continue;

        switch (evt.type) {
            case HEALTH_EVENT_FAULT:
                if (!(*faultFlag)) {
                    Serial.printf("[HEALTH] FAULT detected: %s\n", sensorName);
                    *faultFlag = true;
                    s_faultCount++;
                }
                break;

            case HEALTH_EVENT_STALE:
                if (!(*faultFlag)) {
                    Serial.printf("[HEALTH] STALE data: %s\n", sensorName);
                    *faultFlag = true;
                    s_faultCount++;
                }
                break;

            case HEALTH_EVENT_RECOVERED:
                if (*faultFlag) {
                    Serial.printf("[HEALTH] RECOVERED: %s\n", sensorName);
                    *faultFlag = false;
                    if (s_faultCount > 0) s_faultCount--;
                }
                break;
        }
    }
}

// ============================================================
// CHECK TASK HEARTBEATS
// ============================================================

/**
 * @brief Returns true if ALL critical tasks have heartbeated within threshold.
 *        Called while holding xTelemetryMutex.
 */
static bool heartbeats_ok(const BoatTelemetry &tel) {
    uint32_t now = millis();
    bool ok = true;

#if ENABLE_GPS
    if ((now - tel.gpsLastRun)      > HEARTBEAT_TIMEOUT_MS) {
        Serial.println("[HEALTH] CRITICAL: GPS task heartbeat timeout!");
        ok = false;
    }
#endif
    if ((now - tel.compassLastRun)  > HEARTBEAT_TIMEOUT_MS) {
        Serial.println("[HEALTH] CRITICAL: Compass task heartbeat timeout!");
        ok = false;
    }
    if ((now - tel.imuLastRun)      > HEARTBEAT_TIMEOUT_MS) {
        Serial.println("[HEALTH] CRITICAL: IMU task heartbeat timeout!");
        ok = false;
    }
    if ((now - tel.rtcLastRun)      > HEARTBEAT_TIMEOUT_MS) {
        Serial.println("[HEALTH] CRITICAL: RTC task heartbeat timeout!");
        ok = false;
    }
    if ((now - tel.telemetryLastRun) > HEARTBEAT_TIMEOUT_MS) {
        Serial.println("[HEALTH] CRITICAL: Telemetry task heartbeat timeout!");
        ok = false;
    }

    return ok;
}

// ============================================================
// DETERMINE SYSTEM HEALTH STATE
// ============================================================

static SystemHealthState compute_health_state(const BoatTelemetry &tel) {
    // Heartbeat failure → CRITICAL immediately
    if (!heartbeats_ok(tel)) {
        return HEALTH_CRITICAL;
    }

    // Count invalid sensors from live telemetry (more reliable than queue events)
    uint8_t invalidSensors = 0;
#if ENABLE_GPS
    if (!tel.gpsValid)     invalidSensors++;
#endif
    if (!tel.compassValid) invalidSensors++;
    if (!tel.accelValid)   invalidSensors++;
    if (!tel.rtcValid)     invalidSensors++;

    if (invalidSensors == 0) {
        return HEALTH_HEALTHY;
    } else if (invalidSensors <= 2) {
        return HEALTH_DEGRADED;
    } else {
        return HEALTH_CRITICAL;
    }
}

// ============================================================
// STACK HIGH-WATER MARK REPORT
// ============================================================

static void report_stack_usage(void) {
    // Report current task's own stack
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("[HEALTH] Health task stack HWM: %u words remaining\n", (unsigned)hwm);

    // Report heap
    Serial.printf("[HEALTH] Free heap: %lu bytes | Min ever: %lu bytes\n",
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMinFreeHeap());
}

// ============================================================
// SYSTEM HEALTH TASK FUNCTION
// ============================================================

static void health_task_fn(void *pvParameters) {
    (void)pvParameters;

    Serial.printf("[HEALTH] Task started on Core %d\n", xPortGetCoreID());

    // Give sensor tasks time to initialize before first health check
    vTaskDelay(pdMS_TO_TICKS(3000));

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Step 1: Process any pending events from the health queue
        process_health_queue();

        // Step 2: Read telemetry snapshot and compute health state
        SystemHealthState newState = HEALTH_DEGRADED;

        if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) == pdTRUE) {
            newState = compute_health_state(g_telemetry);
            g_telemetry.healthState = newState;
            xSemaphoreGive(xTelemetryMutex);
        }

        // Step 3: Periodic stack/heap report
        s_cycleCount++;
        if (s_cycleCount >= STACK_REPORT_INTERVAL_CYCLES) {
            s_cycleCount = 0;
            report_stack_usage();
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIOD_HEALTH_MS));
    }
    vTaskDelete(NULL);
}

// ============================================================
// PUBLIC API
// ============================================================

void health_task_start(void) {
    BaseType_t result = xTaskCreatePinnedToCore(
        health_task_fn,
        "Health_Task",
        STACK_HEALTH,
        NULL,
        PRIORITY_HEALTH,
        NULL,
        CORE_HEALTH
    );

    if (result != pdPASS) {
        Serial.println("[HEALTH] FATAL: Failed to create Health task!");
    }
}
