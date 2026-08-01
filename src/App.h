#pragma once

#include "EventBus.h"

namespace jalari
{
    namespace event { class EventBus; }
    namespace node { class NodeManager; }
    namespace radio { class IRadio; }
    namespace queue { class PacketQueue; }
    namespace services { class ForwardingService; class HeartbeatService; class HeartbeatAckManager; class NeighborService; class ReliableLinkService; }
    namespace utils { class Logger; }

    class App final : public event::IEventListener
    {
    public:
        App(radio::IRadio &radio, node::NodeManager &node, queue::PacketQueue &packetQueue, event::EventBus &eventBus,
            services::HeartbeatService &heartbeatService, services::HeartbeatAckManager &heartbeatAckManager,
            services::NeighborService &neighborService, services::ReliableLinkService &reliableLinkService,
            services::ForwardingService &forwardingService, utils::Logger &logger);
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
        utils::Logger &logger_;
        bool initialized_ = false;
        std::uint32_t lastDataMs_ = 0;
        std::uint16_t dataSeq_ = 1;
        std::uint32_t lastHeartbeatTxMs_ = 0;
    };
} // namespace jalari
