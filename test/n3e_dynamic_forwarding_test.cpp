#include <cassert>
#include <cstdint>

#include "Config.h"
#include "DynamicForwardingStrategy.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "IClock.h"
#include "NodeManager.h"
#include "Packet.h"
#include "PacketQueue.h"
#include "RouteTable.h"

class Clock final : public jalari::hal::IClock {
public:
    std::uint32_t millis() const override { return now; }
    std::uint32_t now = 100U;
};

class Listener final : public jalari::event::IEventListener {
public:
    void onEvent(const jalari::event::Event &event) override {
        if (event.type == jalari::event::Type::ForwardingRouteNotFound) ++routeMisses;
        if (event.type == jalari::event::Type::PacketDropped) ++drops;
    }
    int routeMisses = 0;
    int drops = 0;
};

static jalari::config::NetworkConfig network() {
    jalari::config::NetworkConfig config{};
    config.maximumHopCount = 16U;
    return config;
}

static jalari::protocol::Packet dataPacket() {
    jalari::protocol::Packet packet;
    packet.type = jalari::protocol::PacketType::Data;
    packet.sourceId = 0x01U;
    packet.destinationId = 0xFEU;
    packet.previousHopId = 0x01U;
    packet.sequenceNumber = 42U;
    packet.ttl = 8U;
    packet.payload[0] = 0xABU;
    packet.payloadSize = 1U;
    return packet;
}

int main() {
    using namespace jalari;
    const auto config = network();
    Clock clock;

    routing::RouteTable aRoutes(0x01U), bRoutes(0x02U), cRoutes(0x03U);
    assert(aRoutes.updateRoute(0xFEU, 0x02U, 100U, 3U, 1U, clock.now));
    assert(bRoutes.updateRoute(0xFEU, 0x03U, 200U, 2U, 1U, clock.now));
    assert(cRoutes.updateRoute(0xFEU, 0xFEU, 300U, 1U, 1U, clock.now));

    node::NodeManager aNode({0x01U}, node::DeviceRole::BoatNode);
    node::NodeManager bNode({0x02U}, node::DeviceRole::BoatNode);
    node::NodeManager cNode({0x03U}, node::DeviceRole::BoatNode);
    forwarding::DynamicForwardingStrategy aStrategy(aNode.id(), aRoutes, clock);
    forwarding::DynamicForwardingStrategy bStrategy(bNode.id(), bRoutes, clock);
    forwarding::DynamicForwardingStrategy cStrategy(cNode.id(), cRoutes, clock);

    queue::PacketQueue aQueue, bQueue, cQueue;
    event::EventBus aBus, bBus, cBus;
    services::ForwardingService aForwarding(config, aNode, aStrategy, aQueue, aBus);
    services::ForwardingService bForwarding(config, bNode, bStrategy, bQueue, bBus);
    services::ForwardingService cForwarding(config, cNode, cStrategy, cQueue, cBus);

    protocol::Packet packet = dataPacket();
    assert(!aForwarding.processReceived(packet));
    assert(aQueue.dequeue(packet));
    assert(packet.sourceId == 0x01U && packet.destinationId == 0xFEU);
    assert(packet.previousHopId == 0x01U && packet.nextHopId == 0x02U);
    assert(packet.ttl == 7U && packet.hopCount == 1U && packet.sequenceNumber == 42U);
    assert(!bForwarding.processReceived(packet));
    assert(bQueue.dequeue(packet));
    assert(packet.previousHopId == 0x02U && packet.nextHopId == 0x03U);
    assert(packet.ttl == 6U && packet.hopCount == 2U);
    assert(!cForwarding.processReceived(packet));
    assert(cQueue.dequeue(packet));
    assert(packet.previousHopId == 0x03U && packet.nextHopId == 0xFEU);
    assert(packet.ttl == 5U && packet.hopCount == 3U);
    assert(packet.sourceId == 0x01U && packet.destinationId == 0xFEU && packet.payload[0] == 0xABU);

    routing::RouteTable directRoutes(0x04U);
    assert(directRoutes.updateRoute(0xFEU, 0xFEU, 100U, 1U, 1U, clock.now));
    forwarding::DynamicForwardingStrategy directStrategy(0x04U, directRoutes, clock);
    std::uint8_t nextHop = 0U;
    protocol::Packet direct = dataPacket();
    direct.sourceId = 0x04U;
    assert(directStrategy.decide(direct, nextHop) == forwarding::ForwardDecision::Forward && nextHop == 0xFEU);
    direct.destinationId = 0x04U;
    assert(directStrategy.decide(direct, nextHop) == forwarding::ForwardDecision::DeliverLocally);
    direct.destinationId = 0xFFU;
    assert(directStrategy.decide(direct, nextHop) == forwarding::ForwardDecision::Drop);

    routing::RouteTable missingRoutes(0x01U, 10U);
    forwarding::DynamicForwardingStrategy missingStrategy(0x01U, missingRoutes, clock);
    protocol::Packet missing = dataPacket();
    std::uint8_t missingNextHop = 0U;
    assert(missingStrategy.decide(missing, missingNextHop) == forwarding::ForwardDecision::RouteNotFound);
    assert(missingRoutes.updateRoute(0xFEU, 0x02U, 100U, 1U, 1U, clock.now));
    clock.now += 10U;
    assert(missingStrategy.decide(missing, missingNextHop) == forwarding::ForwardDecision::RouteNotFound);

    Listener listener;
    event::EventBus missBus;
    assert(missBus.subscribe(listener));
    services::ForwardingService missForwarding(config, aNode, missingStrategy, aQueue, missBus);
    assert(!missForwarding.processReceived(missing));
    assert(aQueue.empty() && listener.routeMisses == 1 && listener.drops == 1);
    return 0;
}
