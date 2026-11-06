#pragma once
// Turns the classifier's class probabilities into alerts. Pure logic (no hardware), so it is unit-tested on the host (test/acoustic_alert_logic_test.cpp).
//  - chainsaw: a sustained sound, so at least 2 of the last 3 classified windows within 60 s must be above the chainsaw threshold; then at most one alert per 60 s.
//  - blast: one sharp event, so a single window above the blast threshold; then at most one alert per 10 s.
//  - fire: a sustained sound too, so at least 2 of the last 3 windows within 60 s must be above the fire threshold; then at most one alert per 10 minutes. The threshold is high on purpose:
//    on 11.6 hours of real background recordings the fire class alone gave about 5 false windows per hour at 0.90 (recall 0.19) and 22 at 0.70 (recall 0.56). A threshold above 1 turns fire alerts off.
//  - wood and wind are never alerted here (they are logged only).
// Thresholds are starting points (AcousticThresholds.h), not calibrated for the field.

#include <cstdint>

namespace terrawatch::acoustic
{
    enum class AlertKind : std::uint8_t { None = 0, Chainsaw = 1, Blast = 2, Fire = 3 };

    struct Alert
    {
        AlertKind kind = AlertKind::None;
        std::uint8_t confidencePercent = 0;       // the class probability, 0..100
    };

    class AlertLogic
    {
    public:
        AlertLogic(float chainsawThreshold, float blastThreshold, float fireThreshold = 2.0f) : chainsawThreshold_(chainsawThreshold), blastThreshold_(blastThreshold), fireThreshold_(fireThreshold) {}
        // probs: softmax over {chainsaw, blast, fire, wood, wind, background}. nowMs: a millisecond clock that may wrap.
        Alert update(const float *probs, std::uint32_t nowMs);

    private:
        static constexpr int kHistory = 3;
        static constexpr std::uint32_t kChainsawWindowMs = 60000U, kChainsawHoldMs = 60000U, kBlastHoldMs = 10000U, kFireWindowMs = 60000U, kFireHoldMs = 600000U;
        float chainsawThreshold_, blastThreshold_, fireThreshold_;
        std::uint32_t hitTimes_[kHistory] = {0, 0, 0};
        bool hit_[kHistory] = {false, false, false};
        int next_ = 0;
        std::uint32_t lastChainsawMs_ = 0, lastBlastMs_ = 0;
        std::uint32_t fireTimes_[kHistory] = {0, 0, 0};
        bool fireHit_[kHistory] = {false, false, false};
        int fireNext_ = 0;
        std::uint32_t lastFireMs_ = 0;
        bool chainsawSent_ = false, blastSent_ = false, fireSent_ = false;
    };
} // namespace terrawatch::acoustic
