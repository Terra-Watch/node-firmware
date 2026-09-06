#pragma once

#include <cstddef>
#include <cstdint>

namespace jalari::hal
{
    class II2cBus
    {
    public:
        virtual ~II2cBus() = default;
        virtual bool begin(int sdaPin, int sclPin) = 0;
        virtual bool writeRead(std::uint8_t address, const std::uint8_t *writeData, std::size_t writeSize,
                               std::uint8_t *readData, std::size_t readSize) = 0;
        virtual bool write(std::uint8_t address, const std::uint8_t *data, std::size_t size) = 0;
    };
} // namespace jalari::hal
