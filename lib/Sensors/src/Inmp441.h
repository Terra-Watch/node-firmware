#pragma once
// INMP441 I2S MEMS microphone. The sensor streams 24-bit samples in a 32-bit I2S slot; this driver configures
// the ESP32-S3 I2S peripheral in RX/master mode (Philips standard, left slot because L/R is tied to GND) and
// converts to signed 16-bit PCM. Uses the legacy driver/i2s.h API.

#include <cstddef>
#include <cstdint>

namespace terrawatch::sensors
{
    class Inmp441
    {
    public:
        // `shift`: right shift applied to the 32-bit sample to reach 16 bit (16 keeps the top 16 bits of the 24-bit data).
        bool begin(int bclkPin, int wsPin, int dataPin, int sampleRate, int shift);
        // Reads up to maxSamples PCM16 samples (blocks up to timeoutMs for the DMA). Returns the number read, or -1 on a driver error.
        int readSamples(std::int16_t *out, std::size_t maxSamples, std::uint32_t timeoutMs);
        bool started() const { return started_; }

    private:
        int shift_ = 16;
        bool started_ = false;
    };
} // namespace terrawatch::sensors
