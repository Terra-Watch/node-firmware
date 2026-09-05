#include "NullForwardingStrategy.h"

namespace jalari::forwarding
{
    ForwardDecision NullForwardingStrategy::decide(const protocol::Packet &, std::uint8_t &nextHopId) const
    {
        nextHopId = 0U;
        return ForwardDecision::DeliverLocally;
    }
} // namespace jalari::forwarding
