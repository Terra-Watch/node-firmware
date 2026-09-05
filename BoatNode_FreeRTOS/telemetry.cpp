/**
 * telemetry.cpp
 * PalavariRaksha — BoatNode FreeRTOS Firmware
 *
 * Implements the global telemetry singleton, mutex/queue creation,
 * and telemetry helper utilities.
 */

#include "telemetry.h"
#include "config.h"
#include <math.h>   // NAN

static inline float clamp_value(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

// ============================================================
// GLOBAL DEFINITIONS
// These are the actual storage for the extern declarations in telemetry.h
// ============================================================

BoatTelemetry     g_telemetry;
SemaphoreHandle_t xTelemetryMutex = NULL;
SemaphoreHandle_t xI2CMutex       = NULL;
QueueHandle_t     xHealthQueue     = NULL;

// ============================================================
// telemetry_init()
// ============================================================

void telemetry_init(void) {
    // Zero the entire struct first
    memset(&g_telemetry, 0, sizeof(g_telemetry));

    // GPS — leave disabled when not physically connected
    g_telemetry.latitude        = NAN;
    g_telemetry.longitude       = NAN;
    g_telemetry.gpsSpeed        = 0.0f;
    g_telemetry.gpsCourse       = 0.0f;
    g_telemetry.gpsFix          = false;
    g_telemetry.satellites      = 0;
    g_telemetry.hdop            = 99.9f;    // Worst possible HDOP = not yet initialized
    g_telemetry.lastGpsUpdateMs = 0;
    g_telemetry.gpsValid        = ENABLE_GPS ? false : true;

    // Compass
    g_telemetry.compassHeading  = 0.0f;
    g_telemetry.magneticX       = 0.0f;
    g_telemetry.magneticY       = 0.0f;
    g_telemetry.magneticZ       = 0.0f;
    g_telemetry.compassValid    = false;

    // IMU
    g_telemetry.accelX          = 0.0f;
    g_telemetry.accelY          = 0.0f;
    g_telemetry.accelZ          = 0.0f;    // Will become ~1.0g when sensor reads
    g_telemetry.accelValid      = false;

    // RTC
    g_telemetry.rtcValid        = false;
    g_telemetry.timestamp       = 0;
    strncpy(g_telemetry.timeString, "0000-00-00 00:00:00", sizeof(g_telemetry.timeString));

    // Local safety analysis defaults
    g_telemetry.drLatitude      = NAN;
    g_telemetry.drLongitude     = NAN;
    g_telemetry.gpsNoiseDetected = false;
    g_telemetry.gpsNoiseScore    = 100.0f;
    g_telemetry.gpsLossDetected  = false;
    g_telemetry.deadReckoningActive = false;
    g_telemetry.imblWarning      = false;
    g_telemetry.imblCritical     = false;
    g_telemetry.distanceToBoundaryM = 0.0f;
    g_telemetry.boundaryBufferM  = IMBL_CRITICAL_DISTANCE_M;
    g_telemetry.impactDetected   = false;
    g_telemetry.impactPeakG      = 0.0f;
    g_telemetry.impactSeverity   = 0.0f;
    g_telemetry.fusionHealthy    = false;
    g_telemetry.fusedQualityScore = 0.0f;

    // System
    g_telemetry.sensorFault     = true;    // Assume fault until sensors confirm
    g_telemetry.healthState     = HEALTH_DEGRADED;

    // Heartbeats — set to current time so they don't immediately trigger watchdog
    uint32_t now = millis();
    g_telemetry.gpsLastRun      = now;
    g_telemetry.compassLastRun  = now;
    g_telemetry.imuLastRun      = now;
    g_telemetry.rtcLastRun      = now;
    g_telemetry.telemetryLastRun = now;
}

// ============================================================
// telemetry_create_sync_objects()
// ============================================================

bool telemetry_create_sync_objects(void) {
    // Create the telemetry mutex
    xTelemetryMutex = xSemaphoreCreateMutex();
    if (xTelemetryMutex == NULL) {
        Serial.println("[TELEMETRY] FATAL: Failed to create xTelemetryMutex");
        return false;
    }

    // Create the I2C bus mutex
    xI2CMutex = xSemaphoreCreateMutex();
    if (xI2CMutex == NULL) {
        Serial.println("[TELEMETRY] FATAL: Failed to create xI2CMutex");
        return false;
    }

    // Create the health event queue
    // sizeof(HealthEvent) = 2 bytes (two uint8_t fields)
    xHealthQueue = xQueueCreate(HEALTH_QUEUE_LENGTH, sizeof(HealthEvent));
    if (xHealthQueue == NULL) {
        Serial.println("[TELEMETRY] FATAL: Failed to create xHealthQueue");
        return false;
    }

    Serial.println("[TELEMETRY] Sync objects created: mutex(telemetry), mutex(I2C), queue(health)");
    return true;
}

// ============================================================
// telemetry_update_rollup()
// MUST be called while holding xTelemetryMutex.
// ============================================================

void telemetry_update_rollup(void) {
    // Treat GPS as optional when the module is not connected.
    bool gpsOk = ENABLE_GPS ? g_telemetry.gpsValid : true;

    // sensorFault = true if any connected sensor validity flag is false
    g_telemetry.sensorFault = !(gpsOk &&
                                g_telemetry.compassValid &&
                                g_telemetry.accelValid &&
                                g_telemetry.rtcValid);
}

bool telemetry_pack_jalari_payload(uint8_t *buffer, size_t bufferSize, const BoatTelemetry &telemetry, size_t &writtenBytes) {
    if (buffer == nullptr || bufferSize < JALARI_TELEMETRY_PAYLOAD_SIZE) {
        writtenBytes = 0;
        return false;
    }

    memset(buffer, 0, JALARI_TELEMETRY_PAYLOAD_SIZE);

    uint8_t flags = 0;
    if (telemetry.gpsNoiseDetected) flags |= JALARI_FLAG_GPS_NOISE;
    if (telemetry.gpsLossDetected) flags |= JALARI_FLAG_GPS_LOSS;
    if (telemetry.deadReckoningActive) flags |= JALARI_FLAG_DR_ACTIVE;
    if (telemetry.imblWarning) flags |= JALARI_FLAG_IMBL_WARN;
    if (telemetry.imblCritical) flags |= JALARI_FLAG_IMBL_CRIT;
    if (telemetry.impactDetected) flags |= JALARI_FLAG_IMPACT;
    if (telemetry.fusionHealthy) flags |= JALARI_FLAG_FUSION_OK;

    const int32_t latScaled = (int32_t)lround(telemetry.latitude * 1000000.0);
    const int32_t lonScaled = (int32_t)lround(telemetry.longitude * 1000000.0);
    const uint16_t speed10 = (uint16_t)lroundf(telemetry.gpsSpeed * 10.0f);
    const uint16_t course = (uint16_t)lroundf(telemetry.gpsCourse);
    const uint16_t heading = (uint16_t)lroundf(telemetry.compassHeading);
    const int16_t accelX = (int16_t)lroundf(telemetry.accelX * 100.0f);
    const int16_t accelY = (int16_t)lroundf(telemetry.accelY * 100.0f);
    const int16_t accelZ = (int16_t)lroundf(telemetry.accelZ * 100.0f);
    const uint16_t quality = (uint16_t)lroundf(clamp_value(telemetry.fusedQualityScore, 0.0f, 100.0f));

    buffer[0] = JALARI_TELEMETRY_PAYLOAD_VERSION;
    buffer[1] = flags;
    buffer[2] = (uint8_t)(telemetry.timestamp & 0xFFU);
    buffer[3] = (uint8_t)((telemetry.timestamp >> 8U) & 0xFFU);
    buffer[4] = (uint8_t)((telemetry.timestamp >> 16U) & 0xFFU);
    buffer[5] = (uint8_t)((telemetry.timestamp >> 24U) & 0xFFU);

    buffer[6] = (uint8_t)(latScaled & 0xFFU);
    buffer[7] = (uint8_t)((latScaled >> 8U) & 0xFFU);
    buffer[8] = (uint8_t)((latScaled >> 16U) & 0xFFU);
    buffer[9] = (uint8_t)((latScaled >> 24U) & 0xFFU);

    buffer[10] = (uint8_t)(lonScaled & 0xFFU);
    buffer[11] = (uint8_t)((lonScaled >> 8U) & 0xFFU);
    buffer[12] = (uint8_t)((lonScaled >> 16U) & 0xFFU);
    buffer[13] = (uint8_t)((lonScaled >> 24U) & 0xFFU);

    buffer[14] = (uint8_t)(speed10 & 0xFFU);
    buffer[15] = (uint8_t)((speed10 >> 8U) & 0xFFU);
    buffer[16] = (uint8_t)(course & 0xFFU);
    buffer[17] = (uint8_t)((course >> 8U) & 0xFFU);
    buffer[18] = (uint8_t)(heading & 0xFFU);
    buffer[19] = (uint8_t)((heading >> 8U) & 0xFFU);
    buffer[20] = (uint8_t)(accelX & 0xFFU);
    buffer[21] = (uint8_t)((accelX >> 8U) & 0xFFU);
    buffer[22] = (uint8_t)(accelY & 0xFFU);
    buffer[23] = (uint8_t)((accelY >> 8U) & 0xFFU);
    buffer[24] = (uint8_t)(accelZ & 0xFFU);
    buffer[25] = (uint8_t)((accelZ >> 8U) & 0xFFU);
    buffer[26] = (uint8_t)(quality & 0xFFU);
    buffer[27] = (uint8_t)((quality >> 8U) & 0xFFU);
    buffer[28] = telemetry.satellites;
    buffer[29] = (uint8_t)telemetry.healthState;
    buffer[30] = 0;
    buffer[31] = 0;

    writtenBytes = JALARI_TELEMETRY_PAYLOAD_SIZE;
    return true;
}
