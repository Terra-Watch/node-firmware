#include "ForwardingService.h"

#include "NodeManager.h"

namespace jalari::services
{
    ForwardingService::ForwardingService(const config::NetworkConfig &config, const node::NodeManager &node,
                                         const forwarding::IForwardingStrategy &strategy,
                                         queue::PacketQueue &packetQueue, event::EventBus &eventBus)
        : config_(config), node_(node), strategy_(strategy), packetQueue_(packetQueue), eventBus_(eventBus) {}

    void ForwardingService::setDtnService(IDtnStoreForwardSink *dtnService) { dtnService_ = dtnService; }

    bool ForwardingService::processReceived(protocol::Packet &packet) const
    {
        if (packet.destinationId == node_.id() || packet.destinationId == 0xFFU) return true;
        std::uint8_t nextHopId = 0U;
        switch (strategy_.decide(packet, nextHopId))
        {
        case forwarding::ForwardDecision::DeliverLocally:
            publishDrop_(event::Type::PacketDropped, packet);
            return false;
        case forwarding::ForwardDecision::Forward:
            packet.nextHopId = nextHopId;
            if (!packet.prepareForForwarding(config_.maximumHopCount))
            {
                publishDrop_(packet.ttl <= 1U ? event::Type::TtlExpired : event::Type::PacketDropped, packet);
                return false;
            }
            packet.previousHopId = node_.id();
            if (!packetQueue_.enqueue(packet)) { publishDrop_(event::Type::PacketDropped, packet); }
            return false;
        case forwarding::ForwardDecision::RouteNotFound:
            publishDrop_(event::Type::ForwardingRouteNotFound, packet);
            if (dtnService_ == nullptr || !dtnService_->enqueue(packet))
                publishDrop_(event::Type::PacketDropped, packet);
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
