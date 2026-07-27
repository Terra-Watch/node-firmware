#include "Crc16.h"

namespace jalari::utils
{

    std::uint16_t Crc16::computeCcitt(const std::uint8_t *data, std::size_t length)
    {
        std::uint16_t crc = 0xFFFF;

        for (std::size_t i = 0; i < length; ++i)
        {
            crc ^= static_cast<std::uint16_t>(data[i]) << 8;

            for (std::uint8_t bit = 0; bit < 8; ++bit)
            {
                const bool msbSet = (crc & 0x8000U) != 0;
                crc <<= 1;
                if (msbSet)
                {
                    crc ^= 0x1021U;
                }
            }
        }

        return crc;
    }

} // namespace jalari::utils
