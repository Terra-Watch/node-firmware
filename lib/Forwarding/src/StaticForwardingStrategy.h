#pragma once

#include "Config.h"
#include "IForwardingStrategy.h"

namespace jalari::forwarding
{
    class StaticForwardingStrategy final : public IForwardingStrategy
    {
    public:
        explicit StaticForwardingStrategy(const config::NetworkConfig &config) : config_(config) {}
        ForwardDecision decide(const protocol::Packet &packet, std::uint8_t &nextHopId) const override;

    private:
        const config::NetworkConfig &config_;
    };
} // namespace jalari::forwarding
