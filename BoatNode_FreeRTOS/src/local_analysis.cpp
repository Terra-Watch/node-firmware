/**
 * local_analysis.cpp
 * PalavariRaksha — Local BoatNode Safety Analysis
 *
 * This module consumes the existing BoatTelemetry snapshot and computes
 * a compact local-safety analysis layer without altering the JALARI
 * protocol or the gateway/backend flow.
 */

#include "local_analysis.h"
#include "telemetry.h"
#include "config.h"

#include <math.h>

namespace {

static const float kEarthMetersPerDegreeLat = 111320.0f;
static const float kPi = 3.14159265358979323846f;
static double s_lastGoodLat = NAN;
static double s_lastGoodLon = NAN;
static float s_lastGoodCourseDeg = 0.0f;
static float s_lastGoodSpeedKts = 0.0f;


static float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static float normalize_heading(float angleDeg) {
    while (angleDeg < 0.0f) angleDeg += 360.0f;
    while (angleDeg >= 360.0f) angleDeg -= 360.0f;
    return angleDeg;
}

static float shorten_distance(float value, float maxValue) {
    if (value < 0.0f) return 0.0f;
    if (value > maxValue) return maxValue;
    return value;
}

static float compute_gps_noise_score(const BoatTelemetry &tel) {
    if (!tel.gpsValid) {
        return 0.0f;
    }

    float hdopPenalty = tel.hdop > 3.5f ? (tel.hdop - 3.5f) * 20.0f : 0.0f;
    float satBonus = tel.satellites >= 8 ? 25.0f : (float)tel.satellites * 3.0f;
    float score = 100.0f - hdopPenalty + satBonus;
    return clampf(score, 0.0f, 100.0f);
}

static void update_gps_quality(const BoatTelemetry &tel) {
    float score = compute_gps_noise_score(tel);
    bool noisy = (tel.gpsValid && (tel.hdop > GPS_NOISE_HDOP_THRESHOLD || tel.satellites < 4));
    g_telemetry.gpsNoiseDetected = noisy;
    g_telemetry.gpsNoiseScore = score;
}

static void update_dead_reckoning(const BoatTelemetry &tel) {
    const uint32_t nowMs = millis();
    const bool gpsFresh = tel.gpsValid && (nowMs - tel.lastGpsUpdateMs) <= GPS_STALE_TIMEOUT_MS;

    if (gpsFresh) {
        g_telemetry.deadReckoningActive = false;
        g_telemetry.gpsLossDetected = false;
        g_telemetry.drLatitude = tel.latitude;
        g_telemetry.drLongitude = tel.longitude;
        s_lastGoodLat = tel.latitude;
        s_lastGoodLon = tel.longitude;
        s_lastGoodCourseDeg = tel.gpsCourse;
        s_lastGoodSpeedKts = tel.gpsSpeed;
        return;
    }

    if (!isnan(tel.latitude) && !isnan(tel.longitude)) {
        s_lastGoodLat = tel.latitude;
        s_lastGoodLon = tel.longitude;
        s_lastGoodCourseDeg = tel.gpsCourse;
        s_lastGoodSpeedKts = tel.gpsSpeed;
    }

    if (isnan(s_lastGoodLat) || isnan(s_lastGoodLon)) {
        g_telemetry.gpsLossDetected = false;
        g_telemetry.deadReckoningActive = false;
        g_telemetry.drLatitude = NAN;
        g_telemetry.drLongitude = NAN;
        return;
    }

    g_telemetry.gpsLossDetected = true;
    g_telemetry.deadReckoningActive = true;

    float headingDeg = s_lastGoodCourseDeg;
    float speedKts = s_lastGoodSpeedKts;
    float deltaSeconds = (float)(nowMs - tel.lastGpsUpdateMs) / 1000.0f;
    if (tel.lastGpsUpdateMs == 0) deltaSeconds = 1.0f;
    if (speedKts < 0.1f) speedKts = 0.0f;

    float distMeters = speedKts * 0.514444f * deltaSeconds;
    float headingRad = headingDeg * (kPi / 180.0f);

    float dLat = (distMeters * cosf(headingRad)) / kEarthMetersPerDegreeLat;
    float dLon = (distMeters * sinf(headingRad)) /
                 (kEarthMetersPerDegreeLat * cosf((float)s_lastGoodLat * (kPi / 180.0f)) + 1e-6f);

    g_telemetry.drLatitude = s_lastGoodLat + (double)dLat;
    g_telemetry.drLongitude = s_lastGoodLon + (double)dLon;
}

static void update_geofencing(const BoatTelemetry &tel) {
    if (!tel.gpsValid || isnan(tel.latitude) || isnan(tel.longitude)) {
        g_telemetry.imblWarning = false;
        g_telemetry.imblCritical = false;
        g_telemetry.distanceToBoundaryM = 0.0f;
        return;
    }

    double latDiff = tel.latitude - IMBL_BOUNDARY_LATITUDE;
    float distM = fabsf((float)(latDiff * kEarthMetersPerDegreeLat));
    g_telemetry.distanceToBoundaryM = distM;

    bool warning = distM <= IMBL_WARNING_DISTANCE_M;
    bool critical = distM <= IMBL_CRITICAL_DISTANCE_M;

    g_telemetry.imblWarning = warning;
    g_telemetry.imblCritical = critical;
    g_telemetry.boundaryBufferM = IMBL_CRITICAL_DISTANCE_M;
}

static void update_impact_detection(const BoatTelemetry &tel) {
    float magnitude = sqrtf(tel.accelX * tel.accelX + tel.accelY * tel.accelY + tel.accelZ * tel.accelZ);
    float peak = magnitude;

    if (magnitude > IMPACT_ACCEL_THRESHOLD_G) {
        g_telemetry.impactDetected = true;
        g_telemetry.impactSeverity = clampf(magnitude / 10.0f, 0.0f, 1.0f);
        g_telemetry.impactPeakG = peak;
    } else {
        g_telemetry.impactDetected = false;
        g_telemetry.impactSeverity = 0.0f;
        g_telemetry.impactPeakG = 0.0f;
    }

    if (g_telemetry.impactPeakG > IMPACT_PEAK_G_THRESHOLD) {
        g_telemetry.impactSeverity = 1.0f;
    }
}

static void update_fusion_quality(const BoatTelemetry &tel) {
    float healthScore = 100.0f;
    if (!tel.gpsValid) healthScore -= 30.0f;
    if (!tel.compassValid) healthScore -= 20.0f;
    if (!tel.accelValid) healthScore -= 25.0f;
    if (!tel.rtcValid) healthScore -= 15.0f;
    if (tel.gpsNoiseDetected) healthScore -= 15.0f;
    if (tel.imblWarning) healthScore -= 10.0f;
    if (tel.impactDetected) healthScore -= 20.0f;

    healthScore = clampf(healthScore, 0.0f, 100.0f);
    g_telemetry.fusionHealthy = healthScore >= 60.0f;
    g_telemetry.fusedQualityScore = healthScore;
}

static void local_analysis_step(void) {
    if (xSemaphoreTake(xTelemetryMutex, pdMS_TO_TICKS(MUTEX_TELEMETRY_TIMEOUT_MS)) != pdTRUE) {
        return;
    }

    BoatTelemetry &telemetry = g_telemetry;
    update_gps_quality(telemetry);
    update_dead_reckoning(telemetry);
    update_geofencing(telemetry);
    update_impact_detection(telemetry);
    update_fusion_quality(telemetry);

    xSemaphoreGive(xTelemetryMutex);
}

static void local_analysis_task_fn(void *pvParameters) {
    (void)pvParameters;
    Serial.printf("[ANALYSIS] Task started on Core %d\n", xPortGetCoreID());
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        local_analysis_step();
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(PERIOD_ANALYSIS_MS));
    }

    vTaskDelete(NULL);
}

}  // namespace

void local_analysis_task_start(void) {
    BaseType_t result = xTaskCreatePinnedToCore(
        local_analysis_task_fn,
        "Analysis_Task",
        STACK_ANALYSIS,
        NULL,
        PRIORITY_ANALYSIS,
        NULL,
        CORE_ANALYSIS
    );

    if (result != pdPASS) {
        Serial.println("[ANALYSIS] FATAL: Failed to create local analysis task!");
    }
}
