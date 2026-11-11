// Relay node: receives LoRa packets and forwards them toward the gateway. No sensors.
#include <Arduino.h>

#include "App.h"
#include "Config.h"
#include "Esp32Clock.h"
#include "Esp32Delay.h"
#include "Esp32SerialConsole.h"
#include "Esp32SpiBus.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "LoRaDriver.h"
#include "Logger.h"
#include "NodeManager.h"
#include "NeighborManager.h"
#include "PacketFactory.h"
#include "PacketQueue.h"
#include "HeartbeatAckManager.h"
#include "HeartbeatService.h"
#include "AckManager.h"
#include "NeighborService.h"
#include "ReliableLinkService.h"
#include "DynamicForwardingStrategy.h"
#include "RouteTable.h"
#include "RouteAdvertisementService.h"
#include "RouteFailureService.h"
#include "DtnStoreForwardService.h"

namespace
{
    using namespace terrawatch;
    constexpr std::uint32_t kGatewayRouteRefreshMs = 20000U; // route lifetime is 60 s
    constexpr std::uint16_t kGatewayRouteMetric = 100U;

    struct RelayComposition
    {
        hal::Esp32SerialConsole console;
        utils::Logger logger{console};
        hal::Esp32Clock clock;
        hal::Esp32Delay delay;
        hal::Esp32SpiBus spiBus;
        node::NodeManager node{config::kDefaultFirmwareConfig.node, node::DeviceRole::FieldNode};
        lora::LoRaDriver radio{config::kDefaultFirmwareConfig.radio, spiBus};
        protocol::PacketFactory packetFactory{node, config::kDefaultFirmwareConfig.network};
        queue::PacketQueue packetQueue;
        event::EventBus eventBus;
        routing::RouteTable routeTable{node.id()};
        forwarding::DynamicForwardingStrategy forwardingStrategy{node.id(), routeTable, clock};
        neighbor::NeighborManager neighborManager;
        reliable::AckManager ackManager{config::kDefaultFirmwareConfig.reliability, node, packetFactory, packetQueue, clock};
        services::HeartbeatService heartbeatService{config::kDefaultFirmwareConfig.power, packetFactory, packetQueue, clock};
        services::HeartbeatAckManager heartbeatAckManager{clock, logger, packetQueue, node};
        services::NeighborService neighborService{config::kDefaultFirmwareConfig.network, neighborManager, clock};
        services::ReliableLinkService reliableLinkService{ackManager};
        services::ForwardingService forwardingService{config::kDefaultFirmwareConfig.network, node, forwardingStrategy, packetQueue, eventBus};
        routing::RouteAdvertisementService routeAdvertisementService{
            node.id(), routeTable, neighborManager, packetFactory, packetQueue, clock,
            config::kDefaultFirmwareConfig.network.neighborTimeoutMs};
        routing::RouteFailureService routeFailureService{routeTable};
        services::DtnStoreForwardService dtnService{forwardingStrategy, forwardingService, clock, eventBus};
        App app{radio, node, packetQueue, eventBus, heartbeatService, heartbeatAckManager,
                neighborService, reliableLinkService, nullptr, forwardingService, routeAdvertisementService,
                routeFailureService, dtnService, logger, packetFactory, console};
        std::uint32_t lastRouteRefreshMs = 0;
        std::uint16_t routeVersion = 0;
    };
    RelayComposition &relay() { static RelayComposition composition; return composition; }
}

void setup()
{
    relay().delay.milliseconds(2000U);
    if (!relay().app.begin())
        while (true) relay().delay.milliseconds(1000U);
}

void loop()
{
    auto &r = relay();
    const std::uint32_t now = r.clock.millis();
    // The gateway sends no route advertisements, so the relay keeps a direct route to it alive.
    if (r.routeVersion == 0U || (now - r.lastRouteRefreshMs) >= kGatewayRouteRefreshMs)
    {
        r.lastRouteRefreshMs = now;
        r.routeTable.updateRoute(constants::kGatewayNodeId, constants::kGatewayNodeId, kGatewayRouteMetric, 1U, ++r.routeVersion, now);
    }
    r.app.update();
}
