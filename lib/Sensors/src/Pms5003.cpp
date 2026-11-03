#include "Pms5003.h"

#include <Arduino.h>

namespace terrawatch::sensors
{
    namespace
    {
        constexpr std::uint8_t kFrameLen = 32U;
        std::uint16_t be16(const std::uint8_t *p) { return static_cast<std::uint16_t>((p[0] << 8U) | p[1]); }
    }

    void Pms5003::begin(int rxPin, int txPin, int setPin)
    {
        pinMode(setPin, OUTPUT);
        digitalWrite(setPin, HIGH);
        Serial1.begin(9600, SERIAL_8N1, rxPin, txPin);
    }

    bool Pms5003::read(Pms5003Sample &sample)
    {
        while (Serial1.available() > 0)
        {
            const std::uint8_t b = static_cast<std::uint8_t>(Serial1.read());
            ++bytes_;
            if (len_ == 0 && b != 0x42U) continue;
            if (len_ == 1 && b != 0x4DU) { len_ = 0; continue; }
            buf_[len_++] = b;
            if (len_ < kFrameLen) continue;
            len_ = 0;

            std::uint16_t sum = 0;
            for (std::uint8_t i = 0; i < kFrameLen - 2U; ++i) sum += buf_[i];
            if (sum != be16(&buf_[kFrameLen - 2U])) continue;
            latest_.pm25 = be16(&buf_[12]); // PM2.5 atmospheric
            latest_.pm10 = be16(&buf_[14]);
            fresh_ = true;
        }
        if (!fresh_) return false;
        sample = latest_;
        fresh_ = false;
        return true;
    }
} // namespace terrawatch::sensors
