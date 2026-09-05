/**
 * gps_task.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * GPS Task: reads NEO-6M NMEA data via UART2 using TinyGPSPlus,
 * validates the fix, and updates the GPS portion of BoatTelemetry.
 *
 * Library: TinyGPSPlus (mikalhart/TinyGPSPlus)
 * Reason:  Handles NMEA parsing ($GPRMC, $GPGGA) cleanly in a
 *          non-blocking feed loop. Provides fix validity, lat/lon,
 *          speed, course, satellite count, and HDOP as typed objects.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Initialize GPS hardware and start the GPS FreeRTOS task.
 *        Call once from setup() AFTER telemetry_create_sync_objects().
 */
void gps_task_start(void);
