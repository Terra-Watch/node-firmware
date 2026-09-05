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
    struct FirmwareComposition
    {
        jalari::hal::Esp32SerialConsole console;
        jalari::utils::Logger logger{console};
        jalari::hal::Esp32Clock clock;
        jalari::hal::Esp32Delay delay;
        jalari::hal::Esp32SpiBus spiBus;
        jalari::node::NodeManager node{jalari::config::kDefaultFirmwareConfig.node, jalari::node::DeviceRole::BoatNode};
        jalari::lora::LoRaDriver radio{jalari::config::kDefaultFirmwareConfig.radio, spiBus};
        jalari::protocol::PacketFactory packetFactory{node, jalari::config::kDefaultFirmwareConfig.network};
        jalari::queue::PacketQueue packetQueue;
        jalari::event::EventBus eventBus;
        jalari::routing::RouteTable routeTable{node.id()};
        jalari::forwarding::DynamicForwardingStrategy forwardingStrategy{node.id(), routeTable, clock};
        jalari::neighbor::NeighborManager neighborManager;
        jalari::reliable::AckManager ackManager{jalari::config::kDefaultFirmwareConfig.reliability, node, packetFactory, packetQueue, clock};
        jalari::services::HeartbeatService heartbeatService{jalari::config::kDefaultFirmwareConfig.power, packetFactory, packetQueue, clock};
        jalari::services::HeartbeatAckManager heartbeatAckManager{clock, logger, packetQueue, node};
        jalari::services::NeighborService neighborService{jalari::config::kDefaultFirmwareConfig.network, neighborManager, clock};
        jalari::services::ReliableLinkService reliableLinkService{ackManager};
        jalari::services::ForwardingService forwardingService{jalari::config::kDefaultFirmwareConfig.network, node, forwardingStrategy, packetQueue, eventBus};
        jalari::routing::RouteAdvertisementService routeAdvertisementService{
            node.id(), routeTable, neighborManager, packetFactory, packetQueue, clock,
            jalari::config::kDefaultFirmwareConfig.network.neighborTimeoutMs};
        jalari::routing::RouteFailureService routeFailureService{routeTable};
        jalari::services::DtnStoreForwardService dtnService{forwardingStrategy, forwardingService, clock, eventBus};
        jalari::App app{radio, node, packetQueue, eventBus, heartbeatService, heartbeatAckManager,
                        neighborService, reliableLinkService, forwardingService, routeAdvertisementService,
                        routeFailureService, dtnService, logger, packetFactory, console};
    };
    FirmwareComposition &firmware() { static FirmwareComposition composition; return composition; }
}

void setup()
{
    firmware().delay.milliseconds(2000U);

    if (!firmware().app.begin())
    {
        while (true)
        {
            firmware().delay.milliseconds(1000U);
        }
    }
}

void loop()
{
    firmware().app.update();
}
