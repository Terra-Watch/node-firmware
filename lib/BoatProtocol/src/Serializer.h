#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"
#include "Packet.h"

namespace jalari::protocol
{

    class Serializer
    {
    public:
        static bool serialize(const Packet &packet,
                              std::array<std::uint8_t, constants::kMaxPacketSize> &outBuffer,
                              std::size_t &outSize);
    };

} // namespace jalari::protocol
