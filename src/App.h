#pragma once

#include "EventBus.h"

namespace jalari
{
    namespace event { class EventBus; }
    namespace node { class NodeManager; }
    namespace radio { class IRadio; }
    namespace queue { class PacketQueue; }
    namespace services { class ForwardingService; class HeartbeatService; class HeartbeatAckManager; class NeighborService; class ReliableLinkService; class CrewMessageService; }
    namespace utils { class Logger; }

    class App final : public event::IEventListener
    {
    public:
        App(radio::IRadio &radio, node::NodeManager &node, queue::PacketQueue &packetQueue, event::EventBus &eventBus,
            services::HeartbeatService &heartbeatService, services::HeartbeatAckManager &heartbeatAckManager,
            services::NeighborService &neighborService, services::ReliableLinkService &reliableLinkService,
            services::ForwardingService &forwardingService, services::CrewMessageService &crewMessageService, utils::Logger &logger);
        bool begin();
        void update();
        void onEvent(const event::Event &event) override;

    private:
        void transmitQueued_();
        void processIncoming_();
        radio::IRadio &radio_;
        node::NodeManager &node_;
        queue::PacketQueue &packetQueue_;
        event::EventBus &eventBus_;
        services::HeartbeatService &heartbeatService_;
        services::HeartbeatAckManager &heartbeatAckManager_;
        services::NeighborService &neighborService_;
        services::ReliableLinkService &reliableLinkService_;
        services::ForwardingService &forwardingService_;
        services::CrewMessageService &crewMessageService_;
        utils::Logger &logger_;
        bool initialized_ = false;
    };
} // namespace jalari
