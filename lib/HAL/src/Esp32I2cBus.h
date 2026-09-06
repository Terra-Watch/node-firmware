#pragma once

#include "II2cBus.h"

namespace jalari::hal
{
    class Esp32I2cBus final : public II2cBus
    {
    public:
        bool begin(int sdaPin, int sclPin) override;
        bool writeRead(std::uint8_t address, const std::uint8_t *writeData, std::size_t writeSize,
                       std::uint8_t *readData, std::size_t readSize) override;
        bool write(std::uint8_t address, const std::uint8_t *data, std::size_t size) override;
    };
} // namespace jalari::hal
