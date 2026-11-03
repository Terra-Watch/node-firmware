#include "NullForwardingStrategy.h"

namespace terrawatch::forwarding
{
    ForwardDecision NullForwardingStrategy::decide(const protocol::Packet &, std::uint8_t &nextHopId) const
    {
        nextHopId = 0U;
        return ForwardDecision::DeliverLocally;
    }
} // namespace terrawatch::forwarding
