/**
 * telemetry_task.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * Telemetry Task: reads BoatTelemetry and prints formatted debug
 * output to Serial at approximately 1 Hz. This is purely diagnostic
 * output for development; it does NOT transmit over LoRa (that is Phase 2).
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Start the Telemetry debug-print FreeRTOS task.
 *        Call once from setup() AFTER telemetry_create_sync_objects().
 */
void telemetry_task_start(void);
