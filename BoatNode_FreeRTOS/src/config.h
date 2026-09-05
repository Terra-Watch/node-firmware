/**
 * config.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 * Central configuration: pins, timing constants, task parameters.
 *
 * All tunable values live here so they never need to be hunted across
 * multiple source files.
 */

#pragma once

#include <Arduino.h>

// ============================================================
// HARDWARE: GPIO PINS
// ============================================================

// GPS — NEO-6M on UART2
#define GPS_UART_PORT   2       // ESP32 UART number
#define GPS_RX_PIN      16
#define GPS_TX_PIN      17
#define GPS_BAUD        9600

// I2C Bus (shared: HMC5883L + ADXL345 + DS3231)
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
#define I2C_SPEED_HZ    400000  // 400 kHz Fast mode

// I2C Device Addresses
#define ADDR_HMC5883L   0x1E
#define ADDR_ADXL345    0x53
#define ADDR_DS3231     0x68

// ============================================================
// TASK STACK SIZES (in bytes for xTaskCreate on ESP32)
// ESP32-S3 word size = 4 bytes. The API takes bytes.
// ============================================================
#define STACK_GPS           8192    // Larger: TinyGPSPlus + UART buffer
#define STACK_COMPASS       4096
#define STACK_IMU           4096
#define STACK_RTC           4096
#define STACK_TELEMETRY     6144    // Serial formatting uses stack
#define STACK_HEALTH        4096
#define STACK_ANALYSIS      4096    // Local safety analysis and prediction

// ============================================================
// TASK PRIORITIES (higher number = higher priority on ESP32)
// ESP32 FreeRTOS: configMAX_PRIORITIES is typically 25.
// We use a safe range 1–5 (0 = idle is reserved).
// ============================================================
#define PRIORITY_GPS        4       // Highest — must drain UART
#define PRIORITY_COMPASS    3
#define PRIORITY_IMU        3
#define PRIORITY_RTC        2
#define PRIORITY_TELEMETRY  2
#define PRIORITY_ANALYSIS   2
#define PRIORITY_HEALTH     1       // Lowest — background watchdog

// ============================================================
// TASK CORE ASSIGNMENTS
// Core 0: GPS, Telemetry, Health  (leaves Core 1 for I2C tasks)
// Core 1: Compass, IMU, RTC       (all I2C — same core reduces bus contention)
// ============================================================
#define CORE_GPS            0
#define CORE_COMPASS        1
#define CORE_IMU            1
#define CORE_RTC            1
#define CORE_TELEMETRY      0
#define CORE_ANALYSIS       0
#define CORE_HEALTH         0

// ============================================================
// TASK PERIODS (milliseconds)
// Used with vTaskDelayUntil() for deterministic timing.
// ============================================================
#define PERIOD_GPS_MS           1000    // ~1 Hz — NEO-6M update rate
#define PERIOD_COMPASS_MS       100     // 10 Hz — heading updates
#define PERIOD_IMU_MS           50      // 20 Hz — catches transient motion/impact
#define PERIOD_RTC_MS           1000    // 1 Hz — time changes slowly
#define PERIOD_TELEMETRY_MS     1000    // 1 Hz — Serial debug output
#define PERIOD_ANALYSIS_MS      1000    // Local analysis at 1 Hz for safe updates
#define PERIOD_HEALTH_MS        2000    // 0.5 Hz — watchdog check

/*
 * IMU PERIOD EXPLANATION (50 ms / 20 Hz):
 * - The ADXL345 can sample up to 3200 Hz in hardware.
 * - Phase 1 dataset used 1 Hz CSV logging (too slow for impact events).
 * - The dataset report shows impact events lasting only 2–4 s (slams/collisions).
 * - At 20 Hz (50 ms), we capture at minimum 40 samples per impact event —
 *   sufficient to detect the spike shape.
 * - We do NOT go faster at this stage to avoid I2C bus saturation while
 *   sharing the bus with Compass and RTC.
 * - This can be increased to 100 Hz (10 ms) once the I2C mutex contention
 *   pattern is measured in Phase 2.
 */

// ============================================================
// MUTEX TIMEOUTS
// ============================================================
#define MUTEX_TELEMETRY_TIMEOUT_MS  50  // How long a task waits for telemetry lock
#define MUTEX_I2C_TIMEOUT_MS        100 // How long a task waits for I2C bus

// ============================================================
// SENSOR VALIDATION THRESHOLDS
// ============================================================

// GPS
#define GPS_MAX_SPEED_KNOTS     100.0f  // > 100 knots is unrealistic for a fishing boat
#define GPS_STALE_TIMEOUT_MS    5000    // Mark GPS invalid if no update in 5 s
#define GPS_MIN_SATELLITES      1       // Require at least 1 satellite for valid fix
#define GPS_MAX_HDOP            10.0f   // HDOP > 10 = poor fix quality
#define GPS_NOISE_HDOP_THRESHOLD 3.5f   // HDOP above this indicates noisy fix quality
#define GPS_LOSS_WARNING_MS     3000    // Track lost GPS after short blackout window

#define IMBL_BOUNDARY_LATITUDE            13.1460 // Placeholder IMBL reference line near the route
#define IMBL_WARNING_DISTANCE_M           1000.0f
#define IMBL_CRITICAL_DISTANCE_M          500.0f
#define IMPACT_ACCEL_THRESHOLD_G          2.0f
#define IMPACT_PEAK_G_THRESHOLD           3.0f

// Compass
#define COMPASS_HEADING_MIN     0.0f
#define COMPASS_HEADING_MAX     360.0f

// IMU
#define IMU_MAX_ACCEL_G         20.0f   // > ±20g is almost certainly a sensor fault

// RTC
#define RTC_MIN_VALID_YEAR      2024    // DS3231 year must be >= 2024

// ============================================================
// WATCHDOG / HEARTBEAT THRESHOLD
// ============================================================
#define HEARTBEAT_TIMEOUT_MS    5000    // A task not updating in 5 s is considered hung

// ============================================================
// HEALTH QUEUE
// ============================================================
#define HEALTH_QUEUE_LENGTH     10      // Maximum buffered health events

// ============================================================
// SERIAL DEBUG
// ============================================================
#define SERIAL_BAUD             115200
#define DEBUG_DIVIDER           "=============================="
