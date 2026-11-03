#pragma once
// The audio front end of the sound classifier: 1 s of 16 kHz microphone samples -> a 106 x 100 log-mel patch, computed exactly as the model was trained
// (acoustic-ml/src/frontend_ref.py is the reference and MelConstants.h holds its constants): resample to 32 kHz, pre-emphasis, 1024-point FFT with an 800-sample
// Hann window and a hop of 320, 106 mel bands, ln(x + 1e-5), (x + 4.5) / 5.

#include <cstdint>

namespace terrawatch::acoustic
{
    class MelFrontend
    {
    public:
        static constexpr int kSamples = 16000, kBands = 106, kFrames = 100;

        ~MelFrontend();
        bool begin();                                           // allocates the work buffers; false when memory is short
        // pcm: kSamples signed 16-bit samples. out: kBands * kFrames floats, band-major (out[band * kFrames + frame]).
        void compute(const std::int16_t *pcm, float *out);

    private:
        void fft1024();                                         // in place on re_ / im_
        float *y32_ = nullptr;                                  // the resampled audio, 32000 samples
        float *re_ = nullptr, *im_ = nullptr, *power_ = nullptr;
        float *cos_ = nullptr, *sin_ = nullptr, *window_ = nullptr;
        std::uint16_t *bitrev_ = nullptr;
    };
} // namespace terrawatch::acoustic
