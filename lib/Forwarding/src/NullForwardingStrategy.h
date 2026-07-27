#pragma once

#include "IForwardingStrategy.h"

namespace jalari::forwarding
{
    class NullForwardingStrategy final : public IForwardingStrategy
    {
    public:
        ForwardDecision decide(const protocol::Packet &packet) const override;
    };
} // namespace jalari::forwarding
