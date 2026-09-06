#pragma once

#include <cstdint>

#include "II2cBus.h"

namespace jalari::sensors
{
    struct RtcSample
    {
        bool timeValid = false;
        bool temperatureValid = false;
        std::uint16_t year = 0;
        std::uint8_t month = 0;
        std::uint8_t day = 0;
        std::uint8_t hour = 0;
        std::uint8_t minute = 0;
        std::uint8_t second = 0;
        std::int16_t temperatureCentiC = 0;
    };

    class Ds3231 final
    {
    public:
        explicit Ds3231(hal::II2cBus &bus) : bus_(bus) {}
        bool begin(int sdaPin, int sclPin);
        bool read(RtcSample &sample);
        bool setDateTime(const RtcSample &sample);

    private:
        hal::II2cBus &bus_;
        bool initialized_ = false;
    };
} // namespace jalari::sensors
