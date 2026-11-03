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
#include "TelemetryService.h"
#include "Bme680.h"
#include "Pms5003.h"
#include "AcousticMonitor.h"

#ifndef TW_ACOUSTIC
#define TW_ACOUSTIC 1 // 0 = leave out the INMP441 + MAX4466 sound module
#endif

#if TW_ACOUSTIC && defined(TW_ACOUSTIC_MODEL)
namespace
{
    // The classifier raises an alert on its own task; the main loop turns it into a data packet, so the packet queue is only touched from one task.
    volatile bool g_alertPending = false;
    terrawatch::acoustic::Alert g_alert;
    void onAlert(const terrawatch::acoustic::Alert &alert) { g_alert = alert; g_alertPending = true; }
}
#endif

namespace
{
    struct FirmwareComposition
    {
        terrawatch::hal::Esp32SerialConsole console;
        terrawatch::utils::Logger logger{console};
        terrawatch::hal::Esp32Clock clock;
        terrawatch::hal::Esp32Delay delay;
        terrawatch::hal::Esp32SpiBus spiBus;
        terrawatch::sensors::Bme680 bme;
        terrawatch::sensors::Pms5003 pms;
        terrawatch::node::NodeManager node{terrawatch::config::kDefaultFirmwareConfig.node, terrawatch::node::DeviceRole::FieldNode};
        terrawatch::lora::LoRaDriver radio{terrawatch::config::kDefaultFirmwareConfig.radio, spiBus};
        terrawatch::protocol::PacketFactory packetFactory{node, terrawatch::config::kDefaultFirmwareConfig.network};
        terrawatch::queue::PacketQueue packetQueue;
        terrawatch::event::EventBus eventBus;
        terrawatch::routing::RouteTable routeTable{node.id()};
        terrawatch::forwarding::DynamicForwardingStrategy forwardingStrategy{node.id(), routeTable, clock};
        terrawatch::neighbor::NeighborManager neighborManager;
        terrawatch::reliable::AckManager ackManager{terrawatch::config::kDefaultFirmwareConfig.reliability, node, packetFactory, packetQueue, clock};
        terrawatch::services::HeartbeatService heartbeatService{terrawatch::config::kDefaultFirmwareConfig.power, packetFactory, packetQueue, clock};
        terrawatch::services::HeartbeatAckManager heartbeatAckManager{clock, logger, packetQueue, node};
        terrawatch::services::NeighborService neighborService{terrawatch::config::kDefaultFirmwareConfig.network, neighborManager, clock};
        terrawatch::services::ReliableLinkService reliableLinkService{ackManager};
        terrawatch::services::TelemetryService telemetryService{terrawatch::config::kDefaultFirmwareConfig.power,
                                                            terrawatch::config::kDefaultFirmwareConfig.sensors, bme, pms,
                                                            packetFactory, packetQueue, clock, logger};
        terrawatch::services::ForwardingService forwardingService{terrawatch::config::kDefaultFirmwareConfig.network, node, forwardingStrategy, packetQueue, eventBus};
        terrawatch::routing::RouteAdvertisementService routeAdvertisementService{
            node.id(), routeTable, neighborManager, packetFactory, packetQueue, clock,
            terrawatch::config::kDefaultFirmwareConfig.network.neighborTimeoutMs};
        terrawatch::routing::RouteFailureService routeFailureService{routeTable};
        terrawatch::services::DtnStoreForwardService dtnService{forwardingStrategy, forwardingService, clock, eventBus};
        terrawatch::App app{radio, node, packetQueue, eventBus, heartbeatService, heartbeatAckManager,
                        neighborService, reliableLinkService, &telemetryService, forwardingService, routeAdvertisementService,
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
#if TW_ACOUSTIC
    static terrawatch::acoustic::AcousticMonitor acoustic;
#ifdef TW_ACOUSTIC_MODEL
    terrawatch::acoustic::AcousticMonitor::setAlertSink(onAlert);
#endif
    if (!acoustic.begin()) Serial.println("[ACOUSTIC] module not started");
#endif
}

void loop()
{
    firmware().app.update();
#if TW_ACOUSTIC && defined(TW_ACOUSTIC_MODEL)
    if (g_alertPending)
    {
        g_alertPending = false;
        // Alert payload v1: 'A', version 1, kind (1 chainsaw, 2 blast), confidence in percent. Sent to the gateway like a station reading.
        const std::uint8_t payload[4] = {0x41U, 1U, static_cast<std::uint8_t>(g_alert.kind), g_alert.confidencePercent};
        terrawatch::protocol::Packet packet;
        if (firmware().packetFactory.createData(0xFEU, payload, sizeof(payload), packet) && firmware().packetQueue.enqueue(packet))
            Serial.printf("[ACOUSTIC ALERT TX] kind=%u confidence=%u%%\n", static_cast<unsigned>(g_alert.kind), static_cast<unsigned>(g_alert.confidencePercent));
        else
            Serial.println("[ACOUSTIC ALERT] could not queue the alert packet");
    }
#endif
}


