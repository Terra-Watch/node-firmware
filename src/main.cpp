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
#include "NullForwardingStrategy.h"

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
        jalari::forwarding::NullForwardingStrategy forwardingStrategy;
        jalari::neighbor::NeighborManager neighborManager;
        jalari::reliable::AckManager ackManager{jalari::config::kDefaultFirmwareConfig.reliability, node, packetFactory, packetQueue, clock};
        jalari::services::HeartbeatService heartbeatService{jalari::config::kDefaultFirmwareConfig.power, packetFactory, packetQueue, clock};
        jalari::services::HeartbeatAckManager heartbeatAckManager{clock, logger, packetQueue, node};
        jalari::services::NeighborService neighborService{jalari::config::kDefaultFirmwareConfig.network, neighborManager, clock};
        jalari::services::ReliableLinkService reliableLinkService{ackManager};
        jalari::services::ForwardingService forwardingService{jalari::config::kDefaultFirmwareConfig.network, node, forwardingStrategy, packetQueue, eventBus};
        jalari::App app{radio, node, packetQueue, eventBus, heartbeatService, heartbeatAckManager, neighborService, reliableLinkService, forwardingService, logger};
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
