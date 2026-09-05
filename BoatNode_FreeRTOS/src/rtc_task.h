/**
 * rtc_task.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * RTC Task: reads DS3231 real-time clock via I2C.
 * I2C address: 0x68.
 *
 * Library: RTClib (Adafruit RTClib)
 * Reason:  Provides RTC_DS3231 class with clean DateTime objects,
 *          easy string formatting, and seconds-since-epoch calculation.
 *          The DS3231 is treated as the authoritative clock source
 *          (independent of GPS lock) — matches Phase 1 assumption that
 *          rtc_status is always OK.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Initialize DS3231 and start the RTC FreeRTOS task.
 *        Call once from setup() AFTER telemetry_create_sync_objects().
 */
void rtc_task_start(void);
