#include <cassert>
#include <cstdint>

#include "Config.h"
#include "DtnStoreForwardService.h"
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
        if (event.type == jalari::event::Type::DtnPacketQueued) ++queued;
        if (event.type == jalari::event::Type::DtnPacketDequeued) ++dequeued;
        if (event.type == jalari::event::Type::DtnPacketExpired) ++expired;
        if (event.type == jalari::event::Type::DtnPacketDropped) ++dropped;
        if (event.type == jalari::event::Type::DtnPacketForwarded) ++forwarded;
    }
    int queued = 0, dequeued = 0, expired = 0, dropped = 0, forwarded = 0;
};

static jalari::protocol::Packet packet(std::uint16_t sequence, std::uint8_t destination = 0xFEU) {
    jalari::protocol::Packet result;
    result.type = jalari::protocol::PacketType::Data;
    result.sourceId = 0x01U;
    result.destinationId = destination;
    result.previousHopId = 0x01U;
    result.sequenceNumber = sequence;
    result.ttl = 8U;
    result.hopCount = 0U;
    result.payload[0] = 0xC3U;
    result.payloadSize = 1U;
    return result;
}

int main() {
    using namespace jalari;
    config::NetworkConfig config{};
    config.maximumHopCount = 16U;
    config::NodeConfig nodeConfig{0x01U};
    node::NodeManager node(nodeConfig, node::DeviceRole::BoatNode);
    Clock clock;
    routing::RouteTable routes(node.id(), 1000U);
    forwarding::DynamicForwardingStrategy strategy(node.id(), routes, clock);
    queue::PacketQueue radioQueue;
    event::EventBus bus;
    Listener listener;
    assert(bus.subscribe(listener));
    services::ForwardingService forwarding(config, node, strategy, radioQueue, bus);
    services::DtnStoreForwardService dtn(strategy, forwarding, clock, bus, 100U);
    forwarding.setDtnService(&dtn);

    // No route queues DATA; broadcast is not a DTN destination.
    protocol::Packet first = packet(1U);
    assert(!forwarding.processReceived(first));
    assert(dtn.size() == 1U && radioQueue.empty());
    assert(!dtn.enqueue(packet(1U)) && dtn.size() == 1U);
    assert(!dtn.enqueue(packet(2U, 0xFFU)) && dtn.size() == 1U);

    // A valid route bypasses DTN and uses the ordinary forwarding queue.
    assert(routes.updateRoute(0xFEU, 0x02U, 100U, 1U, 1U, clock.now));
    protocol::Packet direct = packet(3U);
    assert(!forwarding.processReceived(direct));
    assert(dtn.size() == 1U && radioQueue.size() == 1U);
    protocol::Packet transmitted;
    assert(radioQueue.dequeue(transmitted));
    assert(transmitted.nextHopId == 0x02U && transmitted.ttl == 7U && transmitted.hopCount == 1U);

    // The queued packet uses a fresh route, including a changed next hop.
    routes.updateRoute(0xFEU, 0x04U, 90U, 1U, 2U, clock.now);
    dtn.update();
    assert(dtn.empty() && radioQueue.size() == 1U);
    assert(radioQueue.dequeue(transmitted));
    assert(transmitted.sequenceNumber == 1U && transmitted.nextHopId == 0x04U);
    assert(transmitted.sourceId == 0x01U && transmitted.destinationId == 0xFEU &&
           transmitted.payload[0] == 0xC3U && transmitted.ttl == 7U && transmitted.hopCount == 1U);

    // Priority is local metadata: SOS is selected before normal/background.
    routes.invalidateRoute(0xFEU);
    assert(dtn.enqueue(packet(10U), services::DtnPriority::Background));
    assert(dtn.enqueue(packet(11U), services::DtnPriority::Normal));
    assert(dtn.enqueue(packet(12U), services::DtnPriority::Sos));
    routes.updateRoute(0xFEU, 0x02U, 100U, 1U, 3U, clock.now);
    dtn.update();
    assert(radioQueue.size() == 2U);
    assert(radioQueue.dequeue(transmitted) && transmitted.sequenceNumber == 12U);
    assert(radioQueue.dequeue(transmitted) && transmitted.sequenceNumber == 11U);

    // Capacity and eviction are bounded; SOS displaces lower-priority traffic.
    routes.invalidateRoute(0xFEU);
    dtn.update();
    dtn.enqueue(packet(20U), services::DtnPriority::Normal);
    dtn.enqueue(packet(21U), services::DtnPriority::Normal);
    dtn.enqueue(packet(22U), services::DtnPriority::Normal);
    dtn.enqueue(packet(23U), services::DtnPriority::Normal);
    dtn.enqueue(packet(24U), services::DtnPriority::Normal);
    dtn.enqueue(packet(25U), services::DtnPriority::Normal);
    dtn.enqueue(packet(26U), services::DtnPriority::Normal);
    dtn.enqueue(packet(27U), services::DtnPriority::Normal);
    assert(dtn.size() == constants::kDtnQueueCapacity);
    assert(dtn.enqueue(packet(28U), services::DtnPriority::Sos));
    assert(dtn.size() == constants::kDtnQueueCapacity);

    // Expiry reclaims entries and never forwards them.
    clock.now += 100U;
    dtn.update();
    assert(dtn.empty());
    assert(listener.expired > 0);
    assert(listener.forwarded > 0);

    // A later route release works for Gateway without changing wire identity.
    assert(dtn.enqueue(packet(30U)));
    routes.updateRoute(0xFEU, 0x04U, 100U, 1U, 4U, clock.now);
    dtn.update();
    assert(dtn.empty() && radioQueue.size() == 1U);
    assert(radioQueue.dequeue(transmitted) && transmitted.sequenceNumber == 30U && transmitted.nextHopId == 0x04U);
    return 0;
}
