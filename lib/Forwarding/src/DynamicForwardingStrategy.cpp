#include "DynamicForwardingStrategy.h"

namespace jalari::forwarding
{
    ForwardDecision DynamicForwardingStrategy::decide(const protocol::Packet &packet,
                                                       std::uint8_t &nextHopId) const
    {
        nextHopId = 0U;

        if (packet.destinationId == localNodeId_) return ForwardDecision::DeliverLocally;
        if (packet.destinationId == 0U || packet.destinationId == 0xFFU) return ForwardDecision::Drop;

        routing::RouteEntry route;
        if (!routeTable_.lookup(packet.destinationId, clock_.millis(), route))
            return ForwardDecision::RouteNotFound;

        // RouteTable owns freshness and primary-route selection. These wire-level
        // checks keep invalid next hops out of the normal DATA path.
        if (route.nextHopId == 0U || route.nextHopId == localNodeId_ || route.nextHopId == 0xFFU)
            return ForwardDecision::Drop;

        nextHopId = route.nextHopId;
        return ForwardDecision::Forward;
    }
} // namespace jalari::forwarding
