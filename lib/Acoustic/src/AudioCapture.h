#pragma once
// INMP441 audio capture into a circular buffer, so the audio *before* a trigger is
// still available when the MAX4466 detector fires.
//
// SampleRing is portable and host-tested. Inmp441Capture is the ESP32-S3 I2S driver
// (legacy driver/i2s.h - the only I2S API in this framework version).

#include <atomic>
#include <cstddef>
#include <cstdint>

#ifdef ARDUINO
#include "Inmp441.h"
#endif

namespace terrawatch::acoustic {

// Single-writer / single-reader ring of PCM16 samples addressed by a free-running
// (wrapping) sample counter, so a reader can ask for "the N samples ending at
// absolute index E" without locking. copyWindow() fails if the requested span has
// (partially) been overwritten, including while it was being copied.
class SampleRing {
public:
    SampleRing(int16_t* storage, size_t capacity) : buf_(storage), cap_(capacity) {}

    void write(const int16_t* samples, size_t count);          // writer only
    uint32_t total() const { return total_.load(std::memory_order_acquire); }
    size_t capacity() const { return cap_; }

    // Copy `count` samples starting at absolute index `start` into dst.
    bool copyWindow(uint32_t start, size_t count, int16_t* dst) const;

private:
    int16_t* buf_;
    size_t cap_;
    std::atomic<uint32_t> total_{0};
};

struct PcmStats {
    int16_t min = 0;
    int16_t max = 0;
    float mean = 0.0f;
    float rms = 0.0f;      // AC RMS (mean removed)
    size_t count = 0;
};
PcmStats computePcmStats(const int16_t* samples, size_t count);

#ifdef ARDUINO
// Feeds the SampleRing from the sensors::Inmp441 I2S driver (lib/Sensors).
class Inmp441Capture {
public:
    bool begin(int bclkPin, int wsPin, int dataPin, int sampleRate, int shift, SampleRing* ring);
    // Reads whatever the DMA has (blocks up to timeoutMs) and appends to the ring.
    // Returns samples appended, or -1 on driver error.
    int pump(uint32_t timeoutMs);
private:
    SampleRing* ring_ = nullptr;
    sensors::Inmp441 mic_;
};
#endif

}  // namespace terrawatch::acoustic
