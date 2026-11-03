#pragma once

#include <cstdint>

namespace terrawatch::sensors
{
    struct Pms5003Sample
    {
        std::uint16_t pm25 = 0; // ug/m3, atmospheric
        std::uint16_t pm10 = 0;
    };

    // Plantower PMS5003 on UART1. The fan stays on (SET high); sleeping it needs ~30 s warm-up before each reading.
    class Pms5003 final
    {
    public:
        void begin(int rxPin, int txPin, int setPin);
        // Drains the UART. True when a valid frame has been seen since the last successful take; fills the newest.
        bool read(Pms5003Sample &sample);
        std::uint32_t bytesSeen() const { return bytes_; }

    private:
        std::uint8_t buf_[32]{};
        std::uint8_t len_ = 0;
        Pms5003Sample latest_{};
        bool fresh_ = false;
        std::uint32_t bytes_ = 0;
    };
} // namespace terrawatch::sensors
