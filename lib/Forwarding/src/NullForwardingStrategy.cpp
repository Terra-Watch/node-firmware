#include "NullForwardingStrategy.h"

namespace jalari::forwarding
{
    ForwardDecision NullForwardingStrategy::decide(const protocol::Packet &) const
    {
        return ForwardDecision::DeliverLocally;
    }
} // namespace jalari::forwarding
