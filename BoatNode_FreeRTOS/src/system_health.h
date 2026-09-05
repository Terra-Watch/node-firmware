/**
 * system_health.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * System Health Task: monitors sensor validity, task heartbeats,
 * and overall system state. Classifies system as HEALTHY, DEGRADED,
 * or CRITICAL. Drains the xHealthQueue for edge-triggered events.
 *
 * This is a lightweight embedded health monitor — NOT the full
 * Phase 2 Outcome 6 multi-sensor fusion algorithm.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Start the System Health monitoring FreeRTOS task.
 *        Call once from setup() AFTER telemetry_create_sync_objects().
 */
void health_task_start(void);
