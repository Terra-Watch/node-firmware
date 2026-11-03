#include "AcousticTrigger.h"

#include <algorithm>
#include <cmath>

namespace terrawatch::acoustic {

float blockRms(const int16_t* samples, int count) {
    if (samples == nullptr || count <= 0) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < count; ++i) sum += samples[i];
    const double mean = sum / count;
    double acc = 0.0;
    for (int i = 0; i < count; ++i) {
        const double d = samples[i] - mean;
        acc += d * d;
    }
    return static_cast<float>(std::sqrt(acc / count));
}

float crestFactor(float peakAboveMean, float rms) {
    return rms > 0.0f ? peakAboveMean / rms : 0.0f;
}

float EnergyDetector::highThreshold() const {
    if (cfg_.relative) return noiseFloor_ * std::pow(10.0f, cfg_.highDb / 20.0f);
    if (!cfg_.adaptive || !floorInit_) return cfg_.high;
    return std::max(cfg_.high, noiseFloor_ * cfg_.noiseFactor + cfg_.noiseDelta);
}

float EnergyDetector::lowThreshold() const {
    if (cfg_.relative) return noiseFloor_ * std::pow(10.0f, cfg_.lowDb / 20.0f);
    if (!cfg_.adaptive || !floorInit_) return cfg_.low;
    // Keep the configured HIGH:LOW ratio so hysteresis scales with the floor.
    return std::max(cfg_.low, highThreshold() * (cfg_.low / cfg_.high));
}

EnergyDetector::Edge EnergyDetector::updateRelative(float rms) {
    const int warmTarget = std::min(cfg_.warmupBlocks, kMaxWarmupBlocks);
    if (warmBlocks_ < warmTarget) {   // collect RMS values; floor = median once complete; no triggering yet
        warm_[warmBlocks_++] = rms;
        floorInit_ = true;
        if (warmBlocks_ < warmTarget) {
            noiseFloor_ += (rms - noiseFloor_) / static_cast<float>(warmBlocks_);   // running mean, display only
        } else {
            float* mid = warm_ + warmTarget / 2;
            std::nth_element(warm_, mid, warm_ + warmTarget);
            noiseFloor_ = *mid;
            warmBlocks_ = cfg_.warmupBlocks;   // mark complete for warmedUp()
            if (cfg_.maxFloor > 0.0f && (noiseFloor_ > cfg_.maxFloor || noiseFloor_ < cfg_.minFloor)) fault_ = true;
        }
        return Edge::None;
    }
    if (fault_) return Edge::None;   // noisy/faulty sensor: never wake the ML
    const float floor = std::max(noiseFloor_, cfg_.minFloor);
    lastDeltaDb_ = 20.0f * std::log10(std::max(rms, 1e-6f) / floor);
    if (active_) {
        if (lastDeltaDb_ < cfg_.lowDb) {
            active_ = false;
            sustainCount_ = 0;
            return Edge::Falling;
        }
        return Edge::None;
    }
    if (lastDeltaDb_ >= cfg_.highDb) {
        active_ = true;
        return Edge::Rising;
    }
    if (lastDeltaDb_ >= cfg_.lowDb) {
        if (++sustainCount_ >= cfg_.sustainBlocks) {
            active_ = true;
            sustainCount_ = 0;
            return Edge::Rising;
        }
    } else {
        sustainCount_ = 0;
        noiseFloor_ += cfg_.floorAlpha * (rms - noiseFloor_);
    }
    return Edge::None;
}

EnergyDetector::Edge EnergyDetector::update(float rms) {
    if (cfg_.relative) return updateRelative(rms);
    Edge edge = Edge::None;
    if (!active_) {
        if (rms >= highThreshold()) {
            active_ = true;
            edge = Edge::Rising;
        } else if (cfg_.adaptive) {
            // Learn the background only while quiet, so an event cannot raise its own threshold.
            if (!floorInit_) {
                noiseFloor_ = rms;
                floorInit_ = true;
            } else {
                noiseFloor_ += cfg_.noiseAlpha * (rms - noiseFloor_);
            }
        }
    } else if (rms < lowThreshold()) {
        active_ = false;
        edge = Edge::Falling;
    }
    return edge;
}

}  // namespace terrawatch::acoustic
