#include <cassert>
#include <cstdint>

#include "Config.h"
#include "DynamicForwardingStrategy.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "IClock.h"
#include "NeighborManager.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "PacketQueue.h"
#include "RouteAdvertisementService.h"
#include "RouteTable.h"

using namespace terrawatch;

class Clock final : public hal::IClock {
public:
    std::uint32_t millis() const override { return now; }
    std::uint32_t now = 1000U;
};

int main() {
    Clock clock;
    config::NetworkConfig network{}; network.maximumHopCount = 16U;
    node::NodeManager node({0x01U}, node::DeviceRole::FieldNode);
    routing::RouteTable routes(0x01U);
    forwarding::DynamicForwardingStrategy strategy(0x01U, routes, clock);
    queue::PacketQueue queue; event::EventBus bus;
    services::ForwardingService forwarding(network, node, strategy, queue, bus);

    protocol::Packet data; data.type = protocol::PacketType::Data; data.sourceId = 0x01U; data.destinationId = 0xFEU; data.previousHopId = 0x01U;
    // No route yet: nobody is named, any node with a route may carry it.
    forwarding.resolveNextHop(data);
    assert(data.nextHopId == constants::kAnyNextHop);
    // With routes: the primary parent is named; after a failover, the very next send names the backup.
    assert(routes.updateRoute(0xFEU, 0x20U, 300U, 2U, 7U, clock.now));
    assert(routes.updateRoute(0xFEU, 0x10U, 150U, 2U, 7U, clock.now));
    forwarding.resolveNextHop(data);
    assert(data.nextHopId == 0x10U);
    assert(routes.promoteBackup(0xFEU, 0x10U));
    forwarding.resolveNextHop(data);
    assert(data.nextHopId == 0x20U);
    // A broadcast is never given a single next hop.
    protocol::Packet broadcast = data; broadcast.destinationId = 0xFFU;
    forwarding.resolveNextHop(broadcast);
    assert(broadcast.nextHopId == constants::kAnyNextHop);

    // The gateway sends no heartbeats, so it is never a measured neighbour; its advertisement is still taken, at link cost 1.
    neighbor::NeighborManager neighbors; queue::PacketQueue advertQueue;
    protocol::PacketFactory factory(node, network);
    routing::RouteTable fresh(0x01U);
    routing::RouteAdvertisementService learning(0x01U, fresh, neighbors, factory, advertQueue, clock, 30000U, 15000U);
    node::NodeManager gatewayNode({0xFEU}, node::DeviceRole::Gateway);
    protocol::PacketFactory gatewayFactory(gatewayNode, network);
    protocol::Packet advert;
    assert(gatewayFactory.createRouteAdvertisement(0xFEU, 1U, 1U, 5U, advert));
    advert.previousHopId = 0xFEU;
    assert(learning.processReceived(advert, clock.now));
    routing::RouteEntry route; assert(fresh.lookup(0xFEU, clock.now, route));
    assert(route.nextHopId == 0xFEU && route.metric == 101U && route.hopCount == 2U);
    // Any other unmeasured neighbour is still refused.
    protocol::Packet other = advert; other.previousHopId = 0x30U; other.sequenceNumber = 9U;
    routing::RouteTable fresh2(0x01U);
    routing::RouteAdvertisementService learning2(0x01U, fresh2, neighbors, factory, advertQueue, clock, 30000U, 15000U);
    assert(!learning2.processReceived(other, clock.now));
    return 0;
}
