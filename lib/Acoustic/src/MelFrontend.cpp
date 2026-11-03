#include "MelFrontend.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MelConstants.h"
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

namespace terrawatch::acoustic
{
    namespace
    {
        constexpr int kFft = mel::kFftSize, kWin = mel::kWindow, kHop = mel::kHop, kN32 = 2 * MelFrontend::kSamples;
        constexpr int kBins = 274;                              // the mel bands read FFT bins 0..273 only

        void *alloc(std::size_t bytes, bool big)
        {
#ifdef ARDUINO
            void *p = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!p && big) p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            return p;
#else
            (void)big; return std::malloc(bytes);
#endif
        }
        // Reflect padding without repeating the edge sample (numpy "reflect"): index into the pre-emphasised signal of length n.
        inline int reflect(int i, int n) { if (i < 0) i = -i; if (i > n - 1) i = 2 * (n - 1) - i; return i; }
    } // namespace

    MelFrontend::~MelFrontend()
    {
        std::free(y32_); std::free(re_); std::free(im_); std::free(power_); std::free(cos_); std::free(sin_); std::free(window_); std::free(bitrev_);
    }

    bool MelFrontend::begin()
    {
        y32_ = static_cast<float *>(alloc(sizeof(float) * kN32, true));
        re_ = static_cast<float *>(alloc(sizeof(float) * kFft, false)); im_ = static_cast<float *>(alloc(sizeof(float) * kFft, false));
        power_ = static_cast<float *>(alloc(sizeof(float) * kBins, false));
        cos_ = static_cast<float *>(alloc(sizeof(float) * kFft / 2, false)); sin_ = static_cast<float *>(alloc(sizeof(float) * kFft / 2, false));
        window_ = static_cast<float *>(alloc(sizeof(float) * kWin, false)); bitrev_ = static_cast<std::uint16_t *>(alloc(sizeof(std::uint16_t) * kFft, false));
        if (!y32_ || !re_ || !im_ || !power_ || !cos_ || !sin_ || !window_ || !bitrev_) return false;
        for (int i = 0; i < kFft / 2; ++i) { const double a = 2.0 * M_PI * i / kFft; cos_[i] = static_cast<float>(std::cos(a)); sin_[i] = static_cast<float>(-std::sin(a)); }
        for (int i = 0; i < kWin; ++i) window_[i] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * M_PI * i / (kWin - 1)));      // symmetric Hann, as torch.hann_window(800, periodic=False)
        for (int i = 0; i < kFft; ++i) { int r = 0; for (int b = 0; b < 10; ++b) if (i & (1 << b)) r |= 1 << (9 - b); bitrev_[i] = static_cast<std::uint16_t>(r); }
        return true;
    }

    void MelFrontend::fft1024()
    {
        for (int i = 0; i < kFft; ++i) { const int j = bitrev_[i]; if (j > i) { std::swap(re_[i], re_[j]); std::swap(im_[i], im_[j]); } }
        for (int len = 2; len <= kFft; len <<= 1)
        {
            const int half = len >> 1, step = kFft / len;
            for (int base = 0; base < kFft; base += len)
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cos_[k * step], wi = sin_[k * step];
                    const int a = base + k, b = a + half;
                    const float tr = re_[b] * wr - im_[b] * wi, ti = re_[b] * wi + im_[b] * wr;
                    re_[b] = re_[a] - tr; im_[b] = im_[a] - ti; re_[a] += tr; im_[a] += ti;
                }
        }
    }

    void MelFrontend::compute(const std::int16_t *pcm, float *out)
    {
        // 1. resample 16 kHz -> 32 kHz: y[2n + p] = sum_k taps[p][k] * x[n + k - 7], zero beyond the ends
        constexpr int K = mel::kTaps, H = K / 2;
        for (int n = 0; n < kSamples; ++n)
            for (int p = 0; p < 2; ++p)
            {
                float acc = 0.0f; const float *t = mel::kResampleTaps + p * K;
                for (int k = 0; k < K; ++k) { const int i = n + k - H; if (i >= 0 && i < kSamples) acc += t[k] * (static_cast<float>(pcm[i]) * (1.0f / 32768.0f)); }
                y32_[2 * n + p] = acc;
            }
        // 2. pre-emphasis pe[n] = y[n + 1] - 0.97 y[n] (length 31999), done on the fly, 3. frames of the reflect-padded signal (centre = true)
        const int n_pe = kN32 - 1; const int frames = 1 + (n_pe + kFft - kFft) / kHop;      // 100
        auto pe = [&](int i) { i = reflect(i - kFft / 2, n_pe); return y32_[i + 1] - 0.97f * y32_[i]; };
        const int lo = (kFft - kWin) / 2;
        for (int t = 0; t < frames; ++t)
        {
            std::memset(re_, 0, sizeof(float) * kFft); std::memset(im_, 0, sizeof(float) * kFft);
            for (int j = 0; j < kWin; ++j) re_[lo + j] = pe(t * kHop + lo + j) * window_[j];
            fft1024();
            for (int k = 0; k < kBins; ++k) power_[k] = re_[k] * re_[k] + im_[k] * im_[k];
            // 4. mel bands, log, normalise
            for (int b = 0; b < kBands; ++b)
            {
                const float *w = mel::kBandWeights + mel::kBandOffset[b]; const int s = mel::kBandStart[b], c = mel::kBandCount[b];
                float acc = 0.0f; for (int i = 0; i < c; ++i) acc += w[i] * power_[s + i];
                out[b * kFrames + t] = (logf(acc + 1e-5f) + 4.5f) * 0.2f;
            }
        }
    }
} // namespace terrawatch::acoustic
