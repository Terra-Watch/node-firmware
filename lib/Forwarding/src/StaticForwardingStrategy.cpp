#include "StaticForwardingStrategy.h"

namespace jalari::forwarding
{
    ForwardDecision StaticForwardingStrategy::decide(const protocol::Packet &packet,
                                                      std::uint8_t &nextHopId) const
    {
        nextHopId = 0U;
        for (std::size_t i = 0; i < config_.staticRouteCount && i < config_.staticRoutes.size(); ++i)
        {
            const auto &route = config_.staticRoutes[i];
            if (route.destinationId == packet.destinationId && route.nextHopId != 0U)
            {
                nextHopId = route.nextHopId;
                return ForwardDecision::Forward;
            }
        }
        return ForwardDecision::RouteNotFound;
    }
} // namespace jalari::forwarding
