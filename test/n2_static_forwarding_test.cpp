#include <cassert>
#include <cstdint>

#include "Config.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "AckManager.h"
#include "IClock.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "PacketQueue.h"
#include "StaticForwardingStrategy.h"

class Listener final : public jalari::event::IEventListener {
public:
    void onEvent(const jalari::event::Event &event) override {
        if (event.type == jalari::event::Type::ForwardingRouteNotFound) ++routeMisses;
        if (event.type == jalari::event::Type::PacketDropped) ++drops;
    }
    int routeMisses = 0;
    int drops = 0;
};

class Clock final : public jalari::hal::IClock {
public:
    std::uint32_t millis() const override { return now; }
    std::uint32_t now = 0;
};

static jalari::config::NetworkConfig routeTo(std::uint8_t nextHop) {
    jalari::config::NetworkConfig config{};
    config.defaultTtl = 8U;
    config.maximumHopCount = 16U;
    config.staticRoutes[0] = {0xFEU, nextHop};
    config.staticRouteCount = 1U;
    return config;
}

int main() {
    using namespace jalari;
    config::NodeConfig aConfig{0x01U}, bConfig{0x02U}, cConfig{0x03U};
    node::NodeManager aNode(aConfig, node::DeviceRole::BoatNode);
    node::NodeManager bNode(bConfig, node::DeviceRole::BoatNode);
    node::NodeManager cNode(cConfig, node::DeviceRole::BoatNode);
    const auto aRoutes = routeTo(0x02U);
    const auto bRoutes = routeTo(0x03U);
    const auto cRoutes = routeTo(0xFEU);
    protocol::PacketFactory aFactory(aNode, aRoutes);
    protocol::PacketFactory bFactory(bNode, bRoutes);

    queue::PacketQueue bQueue;
    event::EventBus bBus;
    Listener listener;
    assert(bBus.subscribe(listener));
    forwarding::StaticForwardingStrategy bStrategy(bRoutes);
    services::ForwardingService bForwarding(bRoutes, bNode, bStrategy, bQueue, bBus);
    Clock bClock;
    reliable::AckManager bReliability(config::kDefaultFirmwareConfig.reliability, bNode, bFactory, bQueue, bClock);
    assert(bReliability.begin(bBus));

    const std::uint8_t payload[] = {0x41U, 0x42U};
    protocol::Packet packet;
    assert(aFactory.createData(0xFEU, payload, sizeof(payload), packet));
    packet.nextHopId = 0x02U; // Local next-hop selection for A's initial transmission.
    assert(packet.sourceId == 0x01U && packet.previousHopId == 0x01U && packet.nextHopId == 0x02U);
    const protocol::Packet packetReceivedAgain = packet;

    // B acknowledges A, then forwards the fresh packet exactly once to C.
    reliable::DuplicatePacketCache cache;
    assert(!cache.isDuplicateAndRemember(packet.sourceId, packet.sequenceNumber, 0U, constants::kDuplicateExpiryMs));
    assert(bReliability.processReceivedData(packet));
    assert(bForwarding.processReceived(packet) == false);
    assert(bQueue.size() == 2U);
    protocol::Packet bAck;
    assert(bQueue.dequeue(bAck));
    assert(bAck.destinationId == 0x01U && bAck.sourceId == 0x02U);
    protocol::Packet forwarded;
    assert(bQueue.dequeue(forwarded));
    assert(forwarded.sourceId == 0x01U && forwarded.destinationId == 0xFEU);
    assert(forwarded.previousHopId == 0x02U && forwarded.nextHopId == 0x03U);
    assert(forwarded.ttl == 7U && forwarded.hopCount == 1U);

    // A retransmission is duplicate-suppressed and must not create B -> C again.
    assert(!bReliability.processReceivedData(packetReceivedAgain));
    assert(bQueue.size() == 1U);
    assert(bQueue.dequeue(bAck) && bAck.destinationId == 0x01U);
    assert(bQueue.empty());

    queue::PacketQueue cQueue;
    event::EventBus cBus;
    forwarding::StaticForwardingStrategy cStrategy(cRoutes);
    services::ForwardingService cForwarding(cRoutes, cNode, cStrategy, cQueue, cBus);
    assert(!cForwarding.processReceived(forwarded));
    protocol::Packet gatewayPacket;
    assert(cQueue.dequeue(gatewayPacket));
    assert(gatewayPacket.sourceId == 0x01U && gatewayPacket.destinationId == 0xFEU);
    assert(gatewayPacket.previousHopId == 0x03U && gatewayPacket.ttl == 6U && gatewayPacket.hopCount == 2U);

    config::NetworkConfig missingRoutes = routeTo(0x02U);
    missingRoutes.staticRouteCount = 0U;
    forwarding::StaticForwardingStrategy missingStrategy(missingRoutes);
    queue::PacketQueue missingQueue;
    event::EventBus missingBus;
    Listener missingListener;
    assert(missingBus.subscribe(missingListener));
    services::ForwardingService missingForwarding(missingRoutes, bNode, missingStrategy, missingQueue, missingBus);
    assert(!missingForwarding.processReceived(packet));
    assert(missingQueue.empty() && missingListener.routeMisses == 1 && missingListener.drops == 1);
    return 0;
}
