#pragma once

#include <cstdint>

namespace terrawatch::sensors
{
    struct Bme680Sample
    {
        std::int16_t temperatureCentiC = 0;
        std::uint16_t humidityCentiPercent = 0;
    };

    // Temperature and humidity only. The gas heater stays off: it self-heats the sensor and costs power.
    class Bme680 final
    {
    public:
        bool begin(int sdaPin, int sclPin);
        bool read(Bme680Sample &sample);
    };
} // namespace terrawatch::sensors
