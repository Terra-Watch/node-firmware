#pragma once

#include <cstddef>
#include <cstdint>

namespace terrawatch::utils
{

    class Crc16
    {
    public:
        static std::uint16_t computeCcitt(const std::uint8_t *data, std::size_t length);
    };

} // namespace terrawatch::utils
