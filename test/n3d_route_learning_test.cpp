#include <cassert>
#include <cstdint>

#include "Config.h"
#include "NeighborManager.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "RouteAdvertisementService.h"

using namespace jalari;

class Clock final : public hal::IClock {
public:
    std::uint32_t millis() const override { return now; }
    std::uint32_t now = 0;
};

static void seedLink(neighbor::NeighborManager &manager, std::uint8_t nodeId, std::uint32_t now = 0U) {
    protocol::Packet heartbeat;
    heartbeat.type = protocol::PacketType::Heartbeat;
    heartbeat.sourceId = nodeId;
    heartbeat.sequenceNumber = 1U;
    manager.observeHeartbeat(heartbeat, -70, 8.0F, now, 8U);
    manager.recordAcknowledgementSuccess(nodeId, now + 1U);
    manager.recordAcknowledgementSuccess(nodeId, now + 2U);
}

static routing::RouteEntry lookup(routing::RouteTable &table, std::uint8_t destination, std::uint32_t now) {
    routing::RouteEntry route;
    assert(table.lookup(destination, now, route));
    return route;
}

int main() {
    config::NodeConfig aConfig{0x01U}, bConfig{0x02U}, cConfig{0x03U}, gatewayConfig{0xFEU};
    node::NodeManager aNode(aConfig, node::DeviceRole::BoatNode);
    node::NodeManager bNode(bConfig, node::DeviceRole::BoatNode);
    node::NodeManager cNode(cConfig, node::DeviceRole::BoatNode);
    node::NodeManager gatewayNode(gatewayConfig, node::DeviceRole::ShoreGateway);
    const config::NetworkConfig network{};
    protocol::PacketFactory aFactory(aNode, network), bFactory(bNode, network), cFactory(cNode, network), gatewayFactory(gatewayNode, network);

    // Neighbor discovery, refresh, RSSI/SNR capture, and expiry.
    neighbor::NeighborManager aNeighbors;
    seedLink(aNeighbors, 0x02U);
    assert(aNeighbors.count() == 1U && aNeighbors.find(0x02U)->averageRssi == -70);
    seedLink(aNeighbors, 0x02U, 100U);
    assert(aNeighbors.find(0x02U)->lastSeenMs == 100U);
    neighbor::Neighbor expired;
    assert(aNeighbors.expireOne(30100U, 30000U, expired) && expired.nodeId == 0x02U);
    assert(aNeighbors.count() == 0U);

    // Gateway advertisement: Gateway -> C, then C -> B, then B -> A.
    neighbor::NeighborManager cNeighbors;
    seedLink(cNeighbors, 0xFEU);
    routing::RouteTable cRoutes(0x03U, 60000U);
    Clock cClock;
    queue::PacketQueue cQueue;
    routing::RouteAdvertisementService cLearning(0x03U, cRoutes, cNeighbors, cFactory, cQueue, cClock, 30000U, 15000U);
    protocol::Packet gatewayAdvertisement;
    assert(gatewayFactory.createRouteAdvertisement(0xFEU, 100U, 1U, 1U, gatewayAdvertisement));
    gatewayAdvertisement.previousHopId = 0xFEU;
    assert(cLearning.processReceived(gatewayAdvertisement, 10U));
    auto cRoute = lookup(cRoutes, 0xFEU, 10U);
    assert(cRoute.nextHopId == 0xFEU && cRoute.metric == 200U && cRoute.hopCount == 2U);

    neighbor::NeighborManager bNeighbors;
    seedLink(bNeighbors, 0x03U);
    routing::RouteTable bRoutes(0x02U, 60000U);
    Clock bClock;
    queue::PacketQueue bQueue;
    routing::RouteAdvertisementService bLearning(0x02U, bRoutes, bNeighbors, bFactory, bQueue, bClock, 30000U, 15000U);
    assert(bLearning.processReceived(gatewayAdvertisement, 20U) == false); // no direct Gateway metric at B
    assert(cLearning.advertiseRoute(0xFEU, 30U));
    protocol::Packet bAdvertisement;
    assert(cQueue.dequeue(bAdvertisement));
    bAdvertisement.previousHopId = 0x03U;
    assert(bLearning.processReceived(bAdvertisement, 30U));
    auto bRoute = lookup(bRoutes, 0xFEU, 30U);
    assert(bRoute.nextHopId == 0x03U && bRoute.metric == 300U && bRoute.hopCount == 3U);

    neighbor::NeighborManager aRouteNeighbors;
    seedLink(aRouteNeighbors, 0x02U);
    routing::RouteTable aRoutes(0x01U, 60000U);
    Clock aClock;
    queue::PacketQueue aQueue;
    routing::RouteAdvertisementService aLearning(0x01U, aRoutes, aRouteNeighbors, aFactory, aQueue, aClock, 30000U, 15000U);
    assert(bLearning.advertiseRoute(0xFEU, 40U));
    assert(bQueue.dequeue(bAdvertisement));
    bAdvertisement.previousHopId = 0x02U;
    assert(aLearning.processReceived(bAdvertisement, 50U));
    auto aRoute = lookup(aRoutes, 0xFEU, 50U);
    assert(aRoute.nextHopId == 0x02U && aRoute.metric == 400U && aRoute.hopCount == 4U);

    // Duplicate advertisements do not repeatedly update the table.
    assert(!aLearning.processReceived(bAdvertisement, 51U));

    // Unknown local link metrics reject route learning; malformed advertisements reject too.
    neighbor::NeighborManager unknownNeighbors;
    routing::RouteTable unknownRoutes(0x01U);
    Clock unknownClock;
    queue::PacketQueue unknownQueue;
    routing::RouteAdvertisementService unknownLearning(0x01U, unknownRoutes, unknownNeighbors, aFactory, unknownQueue, unknownClock, 30000U);
    assert(!unknownLearning.processReceived(bAdvertisement, 0U));
    auto malformed = bAdvertisement;
    malformed.payloadSize = 2U;
    assert(!unknownLearning.processReceived(malformed, 1U));

    // Advertisement emission is bounded by the configured interval.
    assert(aLearning.advertiseRoute(0xFEU, 60U));
    assert(!aLearning.advertiseRoute(0xFEU, 61U));
    assert(aQueue.size() == 1U);
    aQueue.clear();
    assert(aLearning.advertiseRoute(0xFEU, 15060U));

    // Loop-like advertisements cannot install self/broadcast routes or excessive hops.
    protocol::Packet loopAdvertisement;
    assert(bFactory.createRouteAdvertisement(0xFEU, 100U, 255U, 2U, loopAdvertisement));
    loopAdvertisement.previousHopId = 0x02U;
    assert(!aLearning.processReceived(loopAdvertisement, 70U));
    assert(!aRoutes.updateRoute(0x01U, 0x02U, 100U, 1U, 1U, 0U));
    assert(!aRoutes.updateRoute(0xFFU, 0x02U, 100U, 1U, 1U, 0U));

    // Route freshness remains available to future DTN/N3E consumers without activating them.
    assert(aRoutes.lookup(0xFEU, 60049U, aRoute));
    assert(!aRoutes.lookup(0xFEU, 60050U, aRoute));
    return 0;
}
