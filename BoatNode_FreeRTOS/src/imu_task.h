/**
 * imu_task.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * IMU Task: reads ADXL345 accelerometer X/Y/Z via I2C.
 * I2C address: 0x53 (SDO pin tied to VCC).
 *
 * Library: Adafruit_ADXL345_U (Adafruit Unified Sensor)
 * Reason:  Official driver for the ADXL345; provides X/Y/Z
 *          acceleration in m/s² via sensor_event_t. Dividing by
 *          SENSORS_GRAVITY_STANDARD (9.80665) converts to g units
 *          to match Phase 1 dataset columns (accel_x/y/z in g).
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Initialize ADXL345 and start the IMU FreeRTOS task.
 *        Call once from setup() AFTER telemetry_create_sync_objects().
 */
void imu_task_start(void);
