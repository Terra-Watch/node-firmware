#pragma once
// Alert thresholds on the softmax probability, from the full validation set of the float model (the value that catches 80% of the validation windows of the class; acoustic-ml, 2026-11-03).
// The int8 model's probabilities are close to the float ones (chainsaw recall 0.81 against 0.82 at these thresholds), but they are starting points, NOT calibrated for the field.
#define ACOUSTIC_THRESHOLD_CHAINSAW 0.1776f
#define ACOUSTIC_THRESHOLD_BLAST 0.9328f
