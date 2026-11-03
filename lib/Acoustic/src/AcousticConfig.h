#pragma once
// Pins and tuning for the sound module (INMP441 + MAX4466). Every value can be overridden with a PlatformIO
// build flag (-D NAME=value). Trigger values are starting points, NOT calibrated for the field.

// ---- Pins -------------------------------------------------------------------
// MAX4466 OUT must be an ADC1 pin (GPIO1..10). GPIO1 failed on the first bench board (read a flat 4095
// with the wire disconnected), so GPIO2 is used.
#ifndef TW_MAX4466_PIN
#define TW_MAX4466_PIN 2
#endif
// INMP441 (L/R tied to GND -> left channel).
#ifndef TW_I2S_BCLK_PIN
#define TW_I2S_BCLK_PIN 6
#endif
#ifndef TW_I2S_WS_PIN
#define TW_I2S_WS_PIN 7
#endif
#ifndef TW_I2S_DATA_PIN
#define TW_I2S_DATA_PIN 8
#endif

// ---- MAX4466 trigger (cheap energy detector) ---------------------------------
#ifndef MAX4466_SAMPLE_RATE
#define MAX4466_SAMPLE_RATE 8000
#endif
// 128 samples at 8 kHz = 16 ms: short enough not to average a gunshot impulse away.
#ifndef MAX4466_BLOCK_SIZE
#define MAX4466_BLOCK_SIZE 128
#endif
// Thresholds are dB above a noise floor learned in a quiet warm-up, so they do not depend on the gain trimmer.
#ifndef ACOUSTIC_TRIGGER_HIGH_DB
#define ACOUSTIC_TRIGGER_HIGH_DB 10.0f   // one block this far above the floor triggers (transient)
#endif
#ifndef ACOUSTIC_TRIGGER_LOW_DB
#define ACOUSTIC_TRIGGER_LOW_DB 6.0f     // re-arm level, and "elevated" level for sustained sound
#endif
#ifndef ACOUSTIC_SUSTAIN_BLOCKS
#define ACOUSTIC_SUSTAIN_BLOCKS 4        // consecutive elevated blocks (about 64 ms) that also trigger
#endif
#ifndef ACOUSTIC_NOISE_WARMUP_MS
#define ACOUSTIC_NOISE_WARMUP_MS 4000    // keep it quiet for this long after every reset: it sets the floor
#endif
// Warm-up floor above this (RMS counts) or below 1 means a noisy or dead input: the trigger stays off until reset.
#ifndef ACOUSTIC_MAX_NOISE_FLOOR_RMS
#define ACOUSTIC_MAX_NOISE_FLOOR_RMS 40.0f
#endif
// Ignore new triggers for this long after one event.
#ifndef ACOUSTIC_COOLDOWN_MS
#define ACOUSTIC_COOLDOWN_MS 5000
#endif

// ---- INMP441 capture ---------------------------------------------------------
#define ACOUSTIC_SAMPLE_RATE 16000
#define ACOUSTIC_WINDOW_SAMPLES 16000    // 1 s window
// Audio kept from before the trigger (so the onset is not lost) and captured after it. PRE + POST = 1000 ms.
#ifndef ACOUSTIC_PRE_MS
#define ACOUSTIC_PRE_MS 250
#endif
#ifndef ACOUSTIC_POST_MS
#define ACOUSTIC_POST_MS 750
#endif
// 24-bit samples sit in a 32-bit slot; >> 16 keeps the top 16 bits. Lower it (e.g. 14) for digital gain.
#ifndef ACOUSTIC_I2S_SHIFT
#define ACOUSTIC_I2S_SHIFT 16
#endif
// Circular buffer length in samples (2 s), allocated once.
#ifndef ACOUSTIC_RING_SAMPLES
#define ACOUSTIC_RING_SAMPLES 32000
#endif

// ---- Logging -----------------------------------------------------------------
#ifndef ACOUSTIC_DEBUG_INTERVAL_MS
#define ACOUSTIC_DEBUG_INTERVAL_MS 1000  // period of the "[ACOUSTIC] RMS=..." status line (0 = off)
#endif
