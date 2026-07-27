#pragma once

#include "Config.h"
#include "EventBus.h"
#include "IForwardingStrategy.h"
#include "PacketQueue.h"

namespace jalari::node { class NodeManager; }

namespace jalari::services
{
    class ForwardingService
    {
    public:
        ForwardingService(const config::NetworkConfig &config, const node::NodeManager &node,
                          const forwarding::IForwardingStrategy &strategy,
                          queue::PacketQueue &packetQueue, event::EventBus &eventBus);
        bool processReceived(protocol::Packet &packet) const;

    private:
        void publishDrop_(event::Type type, const protocol::Packet &packet) const;
        const config::NetworkConfig &config_;
        const node::NodeManager &node_;
        const forwarding::IForwardingStrategy &strategy_;
        queue::PacketQueue &packetQueue_;
        event::EventBus &eventBus_;
    };
} // namespace jalari::services
