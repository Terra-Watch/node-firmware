#include "AudioCapture.h"

#include <algorithm>
#include <cmath>

namespace terrawatch::acoustic {

void SampleRing::write(const int16_t* samples, size_t count) {
    uint32_t t = total_.load(std::memory_order_relaxed);
    for (size_t i = 0; i < count; ++i) buf_[(t + i) % cap_] = samples[i];
    total_.store(t + static_cast<uint32_t>(count), std::memory_order_release);
}

bool SampleRing::copyWindow(uint32_t start, size_t count, int16_t* dst) const {
    if (count > cap_) return false;
    uint32_t t = total_.load(std::memory_order_acquire);
    const uint32_t end = start + static_cast<uint32_t>(count);
    if (static_cast<int32_t>(t - end) < 0) return false;   // not written yet (unsigned-wrap safe)
    if (static_cast<uint32_t>(t - start) > cap_) return false;   // already overwritten
    for (size_t i = 0; i < count; ++i) dst[i] = buf_[(start + i) % cap_];
    // The writer may have lapped the region while we were copying.
    t = total_.load(std::memory_order_acquire);
    return static_cast<uint32_t>(t - start) <= cap_;
}

PcmStats computePcmStats(const int16_t* samples, size_t count) {
    PcmStats s;
    if (samples == nullptr || count == 0) return s;
    double sum = 0;
    s.min = samples[0];
    s.max = samples[0];
    for (size_t i = 0; i < count; ++i) {
        sum += samples[i];
        s.min = std::min(s.min, samples[i]);
        s.max = std::max(s.max, samples[i]);
    }
    const double mean = sum / count;
    double acc = 0;
    for (size_t i = 0; i < count; ++i) {
        const double d = samples[i] - mean;
        acc += d * d;
    }
    s.mean = static_cast<float>(mean);
    s.rms = static_cast<float>(std::sqrt(acc / count));
    s.count = count;
    return s;
}

}  // namespace terrawatch::acoustic

#ifdef ARDUINO
namespace terrawatch::acoustic {

bool Inmp441Capture::begin(int bclkPin, int wsPin, int dataPin, int sampleRate, int shift, SampleRing* ring) {
    ring_ = ring;
    return mic_.begin(bclkPin, wsPin, dataPin, sampleRate, shift);
}

int Inmp441Capture::pump(uint32_t timeoutMs) {
    if (!mic_.started() || ring_ == nullptr) return -1;
    int16_t pcm[128];
    const int n = mic_.readSamples(pcm, 128, timeoutMs);
    if (n > 0) ring_->write(pcm, static_cast<size_t>(n));
    return n;
}

}  // namespace terrawatch::acoustic
#endif  // ARDUINO
