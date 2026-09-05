#pragma once

#include "EventBus.h"

namespace jalari
{
    namespace event { class EventBus; }
    namespace node { class NodeManager; }
    namespace radio { class IRadio; }
    namespace queue { class PacketQueue; }
    namespace routing { class RouteAdvertisementService; class RouteFailureService; }
    namespace services { class ForwardingService; class HeartbeatService; class HeartbeatAckManager; class NeighborService; class ReliableLinkService; class DtnStoreForwardService; class TelemetryService; }
    namespace protocol { class PacketFactory; }
    namespace hal { class IConsole; }
    namespace utils { class Logger; }

    class App final : public event::IEventListener
    {
    public:
        App(radio::IRadio &radio, node::NodeManager &node, queue::PacketQueue &packetQueue, event::EventBus &eventBus,
            services::HeartbeatService &heartbeatService, services::HeartbeatAckManager &heartbeatAckManager,
            services::NeighborService &neighborService, services::ReliableLinkService &reliableLinkService,
            services::TelemetryService &telemetryService,
            services::ForwardingService &forwardingService, routing::RouteAdvertisementService &routeAdvertisementService,
            routing::RouteFailureService &routeFailureService, services::DtnStoreForwardService &dtnService,
            utils::Logger &logger, protocol::PacketFactory &packetFactory, hal::IConsole &console);
        bool begin();
        void update();
        void onEvent(const event::Event &event) override;

    private:
        void transmitQueued_();
        void processIncoming_();
        void processTestCommands_();
        radio::IRadio &radio_;
        node::NodeManager &node_;
        queue::PacketQueue &packetQueue_;
        event::EventBus &eventBus_;
        services::HeartbeatService &heartbeatService_;
        services::HeartbeatAckManager &heartbeatAckManager_;
        services::NeighborService &neighborService_;
        services::ReliableLinkService &reliableLinkService_;
        services::TelemetryService &telemetryService_;
        services::ForwardingService &forwardingService_;
        routing::RouteAdvertisementService &routeAdvertisementService_;
        routing::RouteFailureService &routeFailureService_;
        services::DtnStoreForwardService &dtnService_;
        utils::Logger &logger_;
        protocol::PacketFactory &packetFactory_;
        hal::IConsole &console_;
        std::uint8_t testDestinationId_ = 0U;
        std::uint16_t testRemainingCount_ = 0U;
        std::size_t testPayloadSize_ = 0U;
        std::uint8_t testPayload_[constants::kMaxPayloadSize]{};
        bool initialized_ = false;
    };
} // namespace jalari
