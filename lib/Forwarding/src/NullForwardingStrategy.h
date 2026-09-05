#pragma once

#include "IForwardingStrategy.h"

namespace jalari::forwarding
{
    class NullForwardingStrategy final : public IForwardingStrategy
    {
    public:
        ForwardDecision decide(const protocol::Packet &packet, std::uint8_t &nextHopId) const override;
    };
} // namespace jalari::forwarding
