/**
 * local_analysis.h
 * PalavariRaksha — BoatNode local safety analysis
 *
 * Computes the six local analysis outcomes from the current telemetry snapshot:
 * 1) position / speed / heading analysis
 * 2) predictive IMBL geofencing
 * 3) GPS noise detection and quality scoring
 * 4) GPS-loss detection and dead reckoning
 * 5) abnormal motion and impact detection
 * 6) multi-sensor fusion and health quality score
 */

#pragma once

#include <Arduino.h>

void local_analysis_task_start(void);
