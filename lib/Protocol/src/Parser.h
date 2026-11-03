#pragma once

#include <cstddef>
#include <cstdint>

#include "Packet.h"

namespace terrawatch::protocol
{

    class Parser
    {
    public:
        static bool parse(const std::uint8_t *data, std::size_t size, Packet &outPacket);
    };

} // namespace terrawatch::protocol
