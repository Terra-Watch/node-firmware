/**
 * compass_task.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * Compass Task: reads HMC5883L magnetic heading via I2C.
 * The HMC5883L is the actual sensor — NOT the QMC5883L clone.
 * I2C address: 0x1E.
 *
 * Library: Adafruit_HMC5883_U (Adafruit Unified Sensor)
 * Reason:  Official driver for the real HMC5883L; provides X/Y/Z
 *          magnetic field readings in gauss as sensor_event_t.
 *          The Adafruit Unified Sensor abstraction allows easy
 *          validation through isnan() checks.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Initialize HMC5883L and start the Compass FreeRTOS task.
 *        Call once from setup() AFTER telemetry_create_sync_objects().
 */
void compass_task_start(void);
