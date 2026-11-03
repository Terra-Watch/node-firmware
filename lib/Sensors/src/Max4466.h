#pragma once
// MAX4466 electret microphone amplifier on an ESP32-S3 ADC1 pin, sampled by the ADC digital controller through DMA
// (no per-sample CPU work). The output rides on a ~VCC/2 bias, so the raw ADC value is never used directly: each
// block has its DC (mean) removed and its RMS computed.

#include <cstdint>

namespace terrawatch::sensors
{
    struct BlockStats
    {
        float rms = 0.0f;  // AC RMS of the block (DC removed), in ADC counts
        float mean = 0.0f; // DC level in ADC counts (a healthy MAX4466 idles near 2000 at 3.3 V)
        std::int16_t min = 0;
        std::int16_t max = 0;
    };

    class Max4466
    {
    public:
        // gpio must be an ADC1 pin (GPIO1..10 on the ESP32-S3).
        bool begin(int gpio, int sampleRateHz, int blockSize);
        // Waits up to timeoutMs for one full block. False on timeout or driver error.
        bool readBlock(BlockStats &stats, std::uint32_t timeoutMs);

    private:
        int blockSize_ = 0;
        int filled_ = 0;
        std::int16_t *block_ = nullptr; // allocated once in begin()
        bool started_ = false;
    };
} // namespace terrawatch::sensors
