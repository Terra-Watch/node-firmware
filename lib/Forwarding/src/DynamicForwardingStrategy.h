#pragma once

#include <cstdint>

#include "IClock.h"
#include "IForwardingStrategy.h"
#include "RouteTable.h"

namespace jalari::forwarding
{
    class DynamicForwardingStrategy final : public IForwardingStrategy
    {
    public:
        DynamicForwardingStrategy(std::uint8_t localNodeId, const routing::RouteTable &routeTable,
                                  const hal::IClock &clock)
            : localNodeId_(localNodeId), routeTable_(routeTable), clock_(clock) {}

        ForwardDecision decide(const protocol::Packet &packet, std::uint8_t &nextHopId) const override;

    private:
        const std::uint8_t localNodeId_;
        const routing::RouteTable &routeTable_;
        const hal::IClock &clock_;
    };
} // namespace jalari::forwarding
