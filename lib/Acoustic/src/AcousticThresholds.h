#pragma once
// Alert thresholds on the softmax probability, from the full validation set of the float model (the value that catches 80% of the validation windows of the class; acoustic-ml, 2026-11-03).
// The int8 model's probabilities are close to the float ones (chainsaw recall 0.81 against 0.82 at these thresholds), but they are starting points, NOT calibrated for the field.
#define ACOUSTIC_THRESHOLD_CHAINSAW 0.1776f
#define ACOUSTIC_THRESHOLD_BLAST 0.9328f
// Fire: NOT calibrated for the field. 0.90 is the highest value that still catches some real recordings (recall 0.19 on the 48 real-fire windows of the external test set, about 5 false windows per hour of background);
// with the 2-of-3 rule and a 10 minute pause it should alert rarely. Set it above 1.0 to switch fire alerts off. Revisit after a field test.
#define ACOUSTIC_THRESHOLD_FIRE 0.90f
