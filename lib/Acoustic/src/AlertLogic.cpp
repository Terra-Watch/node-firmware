#include "AlertLogic.h"

namespace terrawatch::acoustic
{
    namespace
    {
        inline std::uint8_t percent(float p) { const int v = static_cast<int>(p * 100.0f + 0.5f); return static_cast<std::uint8_t>(v < 0 ? 0 : v > 100 ? 100 : v); }
        inline bool older(std::uint32_t now, std::uint32_t then, std::uint32_t ms) { return static_cast<std::uint32_t>(now - then) >= ms; }
        inline Alert make(AlertKind kind, float p) { Alert a; a.kind = kind; a.confidencePercent = percent(p); return a; }      // not brace-initialised: the ESP32 toolchain compiles libraries as C++11
    }

    Alert AlertLogic::update(const float *probs, std::uint32_t nowMs)
    {
        // blast: a single sharp event
        if (probs[1] >= blastThreshold_ && (!blastSent_ || older(nowMs, lastBlastMs_, kBlastHoldMs)))
        {
            blastSent_ = true; lastBlastMs_ = nowMs;
            return make(AlertKind::Blast, probs[1]);
        }
        // chainsaw: remember this window, then count the hits among the last 3 that are less than 60 s old
        hit_[next_] = probs[0] >= chainsawThreshold_; hitTimes_[next_] = nowMs; next_ = (next_ + 1) % kHistory;
        int hits = 0; for (int i = 0; i < kHistory; ++i) if (hit_[i] && !older(nowMs, hitTimes_[i], kChainsawWindowMs)) ++hits;
        if (hits >= 2 && (!chainsawSent_ || older(nowMs, lastChainsawMs_, kChainsawHoldMs)))
        {
            chainsawSent_ = true; lastChainsawMs_ = nowMs;
            return make(AlertKind::Chainsaw, probs[0]);
        }
        return {};
    }
} // namespace terrawatch::acoustic
