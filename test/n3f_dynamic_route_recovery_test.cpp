#include <cassert>
#include <cstdint>

#include "DynamicForwardingStrategy.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "IClock.h"
#include "NodeManager.h"
#include "Packet.h"
#include "PacketQueue.h"
#include "RouteFailureService.h"
#include "RouteTable.h"

class Clock final : public jalari::hal::IClock {
public:
    std::uint32_t millis() const override { return now; }
    std::uint32_t now = 100U;
};

class Listener final : public jalari::event::IEventListener {
public:
    void onEvent(const jalari::event::Event &event) override {
        if (event.type == jalari::event::Type::RouteInvalidated) ++invalidated;
        if (event.type == jalari::event::Type::ForwardingRouteNotFound) ++unavailable;
    }
    int invalidated = 0;
    int unavailable = 0;
};

static jalari::protocol::Packet makeData(std::uint16_t sequence = 7U, std::uint8_t ttl = 8U) {
    jalari::protocol::Packet packet;
    packet.type = jalari::protocol::PacketType::Data;
    packet.sourceId = 0x01U;
    packet.destinationId = 0xFEU;
    packet.previousHopId = 0x01U;
    packet.sequenceNumber = sequence;
    packet.ttl = ttl;
    packet.payload[0] = 0x5AU;
    packet.payloadSize = 1U;
    return packet;
}

int main() {
    using namespace jalari;
    Clock clock;
    routing::RouteTable routes(0x01U, 1000U);
    assert(routes.updateRoute(0xFEU, 0x02U, 100U, 3U, 1U, clock.now));
    // One primary route is supported; a second candidate for the same
    // destination is not retained by RouteTable.
    assert(!routes.updateRoute(0xFEU, 0x04U, 200U, 3U, 1U, clock.now));

    forwarding::DynamicForwardingStrategy strategy(0x01U, routes, clock);
    std::uint8_t nextHop = 0U;
    protocol::Packet packet = makeData();
    packet.nextHopId = 0x02U;
    assert(strategy.decide(packet, nextHop) == forwarding::ForwardDecision::Forward && nextHop == 0x02U);

    event::EventBus bus;
    Listener listener;
    assert(bus.subscribe(listener));
    routing::RouteFailureService failures(routes);
    assert(failures.begin(bus));

    // Transient retry and queue failure events do not invalidate a route.
    bus.publish({event::Type::TransmissionRetryQueued, packet});
    bus.publish({event::Type::TransmissionFailed, packet});
    routing::RouteEntry route;
    assert(routes.lookup(0xFEU, clock.now, route)); // route remains present

    // Retry exhaustion is represented by AcknowledgementFailed and invalidates
    // only the route using the failed next hop.
    bus.publish({event::Type::AcknowledgementFailed, packet});
    assert(!routes.lookup(0xFEU, clock.now, route));
    assert(listener.invalidated == 1 && listener.unavailable == 1);
    assert(strategy.decide(packet, nextHop) == forwarding::ForwardDecision::RouteNotFound);

    // No flood or guessed next hop is produced by a route miss.
    config::NetworkConfig config{};
    config.maximumHopCount = 16U;
    config::NodeConfig nodeConfig{0x01U};
    node::NodeManager node(nodeConfig, node::DeviceRole::BoatNode);
    queue::PacketQueue queue;
    event::EventBus forwardingBus;
    services::ForwardingService forwarding(config, node, strategy, queue, forwardingBus);
    assert(!forwarding.processReceived(packet));
    assert(queue.empty());

    // A fresh route is immediately usable by a new packet, and unrelated
    // destinations are not poisoned by the failure.
    assert(routes.updateRoute(0xFEU, 0x04U, 120U, 3U, 2U, clock.now));
    assert(routes.updateRoute(0x10U, 0x03U, 150U, 2U, 1U, clock.now));
    assert(strategy.decide(makeData(8U), nextHop) == forwarding::ForwardDecision::Forward && nextHop == 0x04U);
    protocol::Packet unrelated = makeData(9U);
    unrelated.destinationId = 0x10U;
    assert(routes.lookup(0x10U, clock.now, route) && route.nextHopId == 0x03U);

    // Expiry and TTL boundary remain authoritative; recovery cannot reset them.
    clock.now += 1000U;
    assert(strategy.decide(makeData(10U), nextHop) == forwarding::ForwardDecision::RouteNotFound);
    assert(routes.updateRoute(0xFEU, 0x04U, 120U, 3U, 2U, clock.now));
    protocol::Packet lowTtl = makeData(11U, 1U);
    assert(!forwarding.processReceived(lowTtl));
    assert(queue.empty());
    assert(lowTtl.ttl == 0U && lowTtl.hopCount == 0U);
    return 0;
}
