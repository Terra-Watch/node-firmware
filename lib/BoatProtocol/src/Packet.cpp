#include "Packet.h"

namespace jalari::protocol
{

    bool Packet::setPayload(const std::uint8_t *data, std::size_t size)
    {
        if (size > constants::kMaxPayloadSize)
        {
            return false;
        }

        for (std::size_t i = 0; i < size; ++i)
        {
            payload[i] = data[i];
        }

        payloadSize = size;
        return true;
    }

    bool Packet::prepareForForwarding(std::uint8_t maximumHopCount)
    {
        if (ttl == 0U || hopCount >= maximumHopCount)
        {
            return false;
        }

        --ttl;
        if (ttl == 0U)
        {
            return false;
        }
        ++hopCount;
        previousHopId = sourceId;
        return true;
    }

} // namespace jalari::protocol
