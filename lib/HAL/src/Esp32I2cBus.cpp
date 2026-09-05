#include "Esp32I2cBus.h"

#include <Arduino.h>
#include <Wire.h>

namespace jalari::hal
{
    bool Esp32I2cBus::begin(int sdaPin, int sclPin)
    {
        return Wire.begin(sdaPin, sclPin);
    }

    bool Esp32I2cBus::writeRead(std::uint8_t address, const std::uint8_t *writeData, std::size_t writeSize,
                                std::uint8_t *readData, std::size_t readSize)
    {
        Wire.beginTransmission(address);
        for (std::size_t i = 0; i < writeSize; ++i) Wire.write(writeData[i]);
        if (Wire.endTransmission(false) != 0) return false;
        if (Wire.requestFrom(static_cast<int>(address), static_cast<int>(readSize)) != readSize) return false;
        for (std::size_t i = 0; i < readSize; ++i) readData[i] = static_cast<std::uint8_t>(Wire.read());
        return true;
    }
} // namespace jalari::hal
