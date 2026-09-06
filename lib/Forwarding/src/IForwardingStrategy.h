#pragma once

#include "Packet.h"

namespace jalari::forwarding
{
    enum class ForwardDecision : unsigned char
    {
        DeliverLocally,
        Forward,
        Drop,
        RouteNotFound,
    };

    class IForwardingStrategy
    {
    public:
        virtual ~IForwardingStrategy() = default;
        virtual ForwardDecision decide(const protocol::Packet &packet, std::uint8_t &nextHopId) const = 0;
    };
} // namespace jalari::forwarding
