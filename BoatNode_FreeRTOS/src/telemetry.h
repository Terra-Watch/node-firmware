/**
 * telemetry.h
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * Defines the unified BoatTelemetry structure — the single shared data
 * object that all sensor tasks write to and the Telemetry/Health tasks read.
 *
 * Also declares the global FreeRTOS synchronization handles so every module
 * can include this header and access them.
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ============================================================
// HEALTH EVENT (posted to the health queue by sensor tasks)
// ============================================================

typedef enum : uint8_t {
    SENSOR_GPS = 0,
    SENSOR_COMPASS,
    SENSOR_IMU,
    SENSOR_RTC
} SensorID;

typedef enum : uint8_t {
    HEALTH_EVENT_FAULT = 0,     // Sensor read failed
    HEALTH_EVENT_RECOVERED,     // Sensor is working again
    HEALTH_EVENT_STALE          // Data has not updated within timeout
} HealthEventType;

struct HealthEvent {
    SensorID       sensor;
    HealthEventType type;
};

// ============================================================
// SYSTEM HEALTH STATE
// ============================================================

typedef enum : uint8_t {
    HEALTH_HEALTHY  = 0,
    HEALTH_DEGRADED = 1,
    HEALTH_CRITICAL = 2
} SystemHealthState;

// ============================================================
// UNIFIED BOAT TELEMETRY STRUCTURE
// Protected by xTelemetryMutex. All accesses MUST take the mutex.
// ============================================================

struct BoatTelemetry {

    // --- Timestamp (from DS3231) ---
    uint32_t timestamp;         // Seconds since 2000-01-01 (DS3231 epoch)
    char     timeString[20];    // Human-readable: "YYYY-MM-DD HH:MM:SS\0"

    // --- GPS — NEO-6M ---
    double   latitude;          // Degrees. NaN when no fix.
    double   longitude;         // Degrees. NaN when no fix.
    float    gpsSpeed;          // Speed over ground, KNOTS (matches Phase 1 dataset units)
    float    gpsCourse;         // Course over ground, degrees 0–360
    bool     gpsFix;            // Raw fix flag from NMEA
    uint8_t  satellites;        // Number of satellites in use
    float    hdop;              // Horizontal dilution of precision
    uint32_t lastGpsUpdateMs;   // millis() when last valid fix was received

    // Derived GPS validity (includes staleness check)
    bool     gpsValid;          // false if no fix OR data is stale

    // --- Compass — HMC5883L ---
    float    compassHeading;    // Magnetic heading, degrees 0–360
    float    magneticX;         // Raw X axis (gauss)
    float    magneticY;         // Raw Y axis (gauss)
    float    magneticZ;         // Raw Z axis (gauss)
    bool     compassValid;      // false if I2C read failed or heading out of range

    // --- Accelerometer — ADXL345 ---
    float    accelX;            // g
    float    accelY;            // g
    float    accelZ;            // g (≈ 1.0 g at rest, vertical)
    bool     accelValid;        // false if I2C read failed or values out of range

    // --- RTC — DS3231 ---
    bool     rtcValid;          // false if I2C read failed or year < RTC_MIN_VALID_YEAR

    // --- Overall sensor rollup ---
    bool     sensorFault;       // true if ANY sensor is invalid

    // --- Local safety analysis (Phase 2 outcomes) ---
    double   drLatitude;        // Dead-reckoned latitude estimated during GPS loss
    double   drLongitude;       // Dead-reckoned longitude estimated during GPS loss
    bool     gpsNoiseDetected;  // true when fix quality is poor or noisy
    float    gpsNoiseScore;     // 0..100 quality score; lower is worse
    bool     gpsLossDetected;   // true when GPS blackout is detected
    bool     deadReckoningActive;
    bool     imblWarning;       // true when nearing the placeholder IMBL boundary
    bool     imblCritical;      // true when within the safe buffer around the boundary
    float    distanceToBoundaryM;
    float    boundaryBufferM;
    bool     impactDetected;    // true if abnormal boost/impact pattern is seen
    float    impactPeakG;       // maximum acceleration magnitude during impact window
    float    impactSeverity;    // normalized severity 0..1 or 0..100 maybe reused
    bool     fusionHealthy;     // true if multi-sensor score is acceptable
    float    fusedQualityScore; // overall fused quality score 0..100

    // --- Task heartbeats ---
    // Each task updates its own field with millis() on every execution.
    // System Health Task uses these to detect hung/crashed tasks.
    uint32_t gpsLastRun;
    uint32_t compassLastRun;
    uint32_t imuLastRun;
    uint32_t rtcLastRun;
    uint32_t telemetryLastRun;

    // --- System health state (written by Health Task) ---
    SystemHealthState healthState;

    /*
     * RESERVED FOR PHASE 2 (DO NOT IMPLEMENT YET):
     *
     * // Dead reckoning
     * double   drLatitude;
     * double   drLongitude;
     *
     * // IMBL geofencing
     * bool     imblWarning;
     * float    distanceToBoundary;
     *
     * // Abnormal motion
     * bool     impactDetected;
     * float    impactPeakG;
     *
     * // LoRa/JALARI packet ready flag
     * bool     packetReady;
     */
};

// ============================================================
// GLOBAL SINGLETON TELEMETRY + SYNCHRONIZATION HANDLES
// Defined in telemetry.cpp. Every module includes this header.
// ============================================================

extern BoatTelemetry    g_telemetry;
extern SemaphoreHandle_t xTelemetryMutex;
extern SemaphoreHandle_t xI2CMutex;
extern QueueHandle_t     xHealthQueue;

// ============================================================
// HELPER FUNCTIONS (implemented in telemetry.cpp)
// ============================================================

/**
 * @brief Initialize global telemetry struct to safe defaults.
 *        Call once at startup before creating tasks.
 */
void telemetry_init(void);

/**
 * @brief Create xTelemetryMutex, xI2CMutex, and xHealthQueue.
 *        Returns true on success, false if any allocation fails.
 *        Call once in setup() before creating tasks.
 */
bool telemetry_create_sync_objects(void);

/**
 * @brief Compute and update the sensorFault rollup field.
 *        MUST be called while holding xTelemetryMutex.
 */
void telemetry_update_rollup(void);

/*
 * JALARI payload helper (minimal, protocol-compatible, non-invasive)
 *
 * Serializes the current BoatTelemetry snapshot into a compact binary payload
 * that can be handed to the existing LoRa/JALARI transmit path without
 * changing the packet semantics or the sensor framework.
 *
 * Layout (version 1):
 *   [0]  payloadVersion
 *   [1]  analysisFlags
 *   [2-5] timestamp (uint32 little-endian)
 *   [6-9] latitude int32 (1e6 scale)
 *   [10-13] longitude int32 (1e6 scale)
 *   [14-15] gpsSpeed uint16 (knots * 10)
 *   [16-17] gpsCourse uint16 (degrees)
 *   [18-19] compassHeading uint16 (degrees)
 *   [20-21] accelX int16 (g * 100)
 *   [22-23] accelY int16 (g * 100)
 *   [24-25] accelZ int16 (g * 100)
 *   [26-27] qualityScore uint16 (0..100)
 *   [28]  satellites uint8
 *   [29]  status uint8 (0=healthy, 1=degraded, 2=critical)
 *   [30-31] reserved bytes
 */
#define JALARI_TELEMETRY_PAYLOAD_VERSION 1U
#define JALARI_TELEMETRY_PAYLOAD_SIZE 32U

enum BoatNodeJalariFlags : uint8_t {
    JALARI_FLAG_GPS_NOISE = 1 << 0,
    JALARI_FLAG_GPS_LOSS  = 1 << 1,
    JALARI_FLAG_DR_ACTIVE = 1 << 2,
    JALARI_FLAG_IMBL_WARN = 1 << 3,
    JALARI_FLAG_IMBL_CRIT = 1 << 4,
    JALARI_FLAG_IMPACT    = 1 << 5,
    JALARI_FLAG_FUSION_OK = 1 << 6
};

bool telemetry_pack_jalari_payload(uint8_t *buffer, size_t bufferSize, const BoatTelemetry &telemetry, size_t &writtenBytes);
