#include "ForwardingService.h"

#include "NodeManager.h"

namespace jalari::services
{
    ForwardingService::ForwardingService(const config::NetworkConfig &config, const node::NodeManager &node,
                                         const forwarding::IForwardingStrategy &strategy,
                                         queue::PacketQueue &packetQueue, event::EventBus &eventBus)
        : config_(config), node_(node), strategy_(strategy), packetQueue_(packetQueue), eventBus_(eventBus) {}

    bool ForwardingService::processReceived(protocol::Packet &packet) const
    {
        if (packet.ttl == 0U) { publishDrop_(event::Type::TtlExpired, packet); return false; }
        if (packet.destinationId == node_.id() || packet.destinationId == 0xFFU) return true;
        switch (strategy_.decide(packet))
        {
        case forwarding::ForwardDecision::DeliverLocally:
            publishDrop_(event::Type::PacketDropped, packet);
            return false;
        case forwarding::ForwardDecision::Forward:
            if (!packet.prepareForForwarding(config_.maximumHopCount))
            {
                publishDrop_(packet.ttl <= 1U ? event::Type::TtlExpired : event::Type::PacketDropped, packet);
                return false;
            }
            if (!packetQueue_.enqueue(packet)) { publishDrop_(event::Type::PacketDropped, packet); }
            return false;
        case forwarding::ForwardDecision::Drop:
        default:
            publishDrop_(event::Type::PacketDropped, packet);
            return false;
        }
    }

    void ForwardingService::publishDrop_(event::Type type, const protocol::Packet &packet) const
    {
        eventBus_.publish({type, packet});
        if (type == event::Type::TtlExpired) eventBus_.publish({event::Type::PacketDropped, packet});
    }
} // namespace jalari::services
