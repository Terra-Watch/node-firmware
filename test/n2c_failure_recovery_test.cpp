#include <cassert>
#include <cstdint>

#include "AckManager.h"
#include "Config.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "PacketQueue.h"
#include "StaticForwardingStrategy.h"

using namespace jalari;

class Clock final : public hal::IClock {
public:
    std::uint32_t millis() const override { return now; }
    std::uint32_t now = 0;
};

class Events final : public event::IEventListener {
public:
    void onEvent(const event::Event &event) override {
        switch (event.type) {
        case event::Type::TransmissionRetryQueued: ++retries; break;
        case event::Type::TransmissionFailed: ++transmissionFailures; break;
        case event::Type::AcknowledgementFailed: ++ackFailures; break;
        case event::Type::DuplicateSuppressed: ++duplicates; break;
        case event::Type::ForwardingRouteNotFound: ++routeFailures; break;
        case event::Type::TtlExpired: ++ttlFailures; break;
        case event::Type::PacketDropped: ++drops; break;
        default: break;
        }
    }
    int retries = 0;
    int transmissionFailures = 0;
    int ackFailures = 0;
    int duplicates = 0;
    int routeFailures = 0;
    int ttlFailures = 0;
    int drops = 0;
};

static config::NetworkConfig routeTo(std::uint8_t nextHop) {
    config::NetworkConfig config{};
    config.defaultTtl = 8U;
    config.maximumHopCount = 2U;
    config.staticRoutes[0] = config::NetworkConfig::StaticRoute(0xFEU, nextHop);
    config.staticRouteCount = 1U;
    return config;
}

static protocol::Packet makeData(protocol::PacketFactory &factory) {
    protocol::Packet packet;
    const std::uint8_t payload[] = {0xAAU};
    assert(factory.createData(0xFEU, payload, sizeof(payload), packet));
    return packet;
}

int main() {
    config::NodeConfig aConfig{0x01U}, bConfig{0x02U}, cConfig{0x03U};
    node::NodeManager aNode(aConfig, node::DeviceRole::BoatNode);
    node::NodeManager bNode(bConfig, node::DeviceRole::BoatNode);
    node::NodeManager cNode(cConfig, node::DeviceRole::BoatNode);
    const auto aRoutes = routeTo(0x02U);
    const auto bRoutes = routeTo(0x03U);
    const auto cRoutes = routeTo(0xFEU);
    protocol::PacketFactory aFactory(aNode, aRoutes);
    protocol::PacketFactory bFactory(bNode, bRoutes);
    protocol::PacketFactory cFactory(cNode, cRoutes);

    // Baseline and ACK-loss recovery at B: duplicate DATA is re-ACKed but not forwarded twice.
    queue::PacketQueue bQueue;
    event::EventBus bBus;
    Events bEvents;
    assert(bBus.subscribe(bEvents));
    Clock bClock;
    reliable::AckManager bAck(config::kDefaultFirmwareConfig.reliability, bNode, bFactory, bQueue, bClock);
    assert(bAck.begin(bBus));
    forwarding::StaticForwardingStrategy bStrategy(bRoutes);
    services::ForwardingService bForward(bRoutes, bNode, bStrategy, bQueue, bBus);
    auto packet = makeData(aFactory);
    const auto original = packet;
    assert(bAck.processReceivedData(packet));
    assert(!bForward.processReceived(packet));
    protocol::Packet hopAck;
    assert(bQueue.dequeue(hopAck) && hopAck.destinationId == 0x01U);
    protocol::Packet forwarded;
    assert(bQueue.dequeue(forwarded));
    assert(forwarded.sourceId == 0x01U && forwarded.destinationId == 0xFEU);
    assert(forwarded.previousHopId == 0x02U && forwarded.ttl == 7U && forwarded.hopCount == 1U);
    assert(!bAck.processReceivedData(original));
    assert(bEvents.duplicates == 1);
    assert(bQueue.dequeue(hopAck) && hopAck.destinationId == 0x01U);
    assert(bQueue.empty());

    // ACK-loss recovery at C: C re-ACKs B and does not forward the duplicate.
    queue::PacketQueue cQueue;
    event::EventBus cBus;
    Events cEvents;
    assert(cBus.subscribe(cEvents));
    Clock cClock;
    reliable::AckManager cAck(config::kDefaultFirmwareConfig.reliability, cNode, cFactory, cQueue, cClock);
    assert(cAck.begin(cBus));
    forwarding::StaticForwardingStrategy cStrategy(cRoutes);
    services::ForwardingService cForward(cRoutes, cNode, cStrategy, cQueue, cBus);
    const auto cInputAgain = forwarded;
    assert(cAck.processReceivedData(forwarded));
    assert(!cForward.processReceived(forwarded));
    assert(cQueue.dequeue(hopAck) && hopAck.destinationId == 0x02U);
    protocol::Packet gatewayPacket;
    assert(cQueue.dequeue(gatewayPacket));
    assert(!cAck.processReceivedData(cInputAgain));
    assert(cEvents.duplicates == 1);
    assert(cQueue.dequeue(hopAck) && hopAck.destinationId == 0x02U);
    assert(cQueue.empty());

    // Lost DATA / unavailable next hop: bounded retries, then one terminal failure.
    queue::PacketQueue aQueue;
    event::EventBus aBus;
    Events aEvents;
    assert(aBus.subscribe(aEvents));
    Clock aClock;
    reliable::AckManager aAck(config::kDefaultFirmwareConfig.reliability, aNode, aFactory, aQueue, aClock);
    assert(aAck.begin(aBus));
    auto failedPacket = makeData(aFactory);
    failedPacket.nextHopId = 0x02U;
    aBus.publish({event::Type::PacketSent, failedPacket});
    aClock.now += 1000U;
    aAck.update();
    for (std::uint8_t attempt = 0; attempt < 3U; ++attempt) {
        protocol::Packet retry;
        assert(aQueue.dequeue(retry));
        aAck.onTransmissionFailed(retry);
    }
    // The third failed retry exhausts the configured retry budget.
    assert(aQueue.empty() && aAck.pendingCount() == 0U);
    assert(aEvents.retries == 3 && aEvents.transmissionFailures == 1 && aEvents.ackFailures == 1);

    // Missing route is distinct from link failure and never enqueues a packet.
    config::NetworkConfig missingRoutes = routeTo(0x03U);
    missingRoutes.staticRouteCount = 0U;
    forwarding::StaticForwardingStrategy missingStrategy(missingRoutes);
    queue::PacketQueue missingQueue;
    event::EventBus missingBus;
    Events missingEvents;
    assert(missingBus.subscribe(missingEvents));
    services::ForwardingService missingForward(missingRoutes, bNode, missingStrategy, missingQueue, missingBus);
    auto missingPacket = makeData(aFactory);
    assert(!missingForward.processReceived(missingPacket));
    assert(missingQueue.empty() && missingEvents.routeFailures == 1 && missingEvents.drops == 1);
    missingRoutes.staticRoutes[0] = config::NetworkConfig::StaticRoute(0xFEU, 0x03U);
    missingRoutes.staticRouteCount = 1U;
    auto recoveredPacket = makeData(aFactory);
    assert(!missingForward.processReceived(recoveredPacket));
    assert(missingQueue.dequeue(forwarded) && forwarded.nextHopId == 0x03U);

    // TTL=0/1 and hop-count limit are deterministic and bounded.
    auto ttlZero = makeData(aFactory);
    ttlZero.ttl = 0U;
    assert(!bForward.processReceived(ttlZero));
    auto ttlOne = makeData(aFactory);
    ttlOne.ttl = 1U;
    assert(!bForward.processReceived(ttlOne));
    assert(bEvents.ttlFailures >= 2 && bEvents.drops >= 2);
    auto hopLimit = makeData(aFactory);
    hopLimit.hopCount = 2U;
    assert(!bForward.processReceived(hopLimit));
    assert(bEvents.ttlFailures >= 2);

    // A static B <-> C loop terminates by the existing TTL guard.
    const auto loopRoutes = routeTo(0x03U);
    const auto reverseLoopRoutes = routeTo(0x02U);
    forwarding::StaticForwardingStrategy loopBStrategy(loopRoutes);
    forwarding::StaticForwardingStrategy loopCStrategy(reverseLoopRoutes);
    queue::PacketQueue loopQueue;
    event::EventBus loopBus;
    Events loopEvents;
    assert(loopBus.subscribe(loopEvents));
    services::ForwardingService loopB(loopRoutes, bNode, loopBStrategy, loopQueue, loopBus);
    services::ForwardingService loopC(reverseLoopRoutes, cNode, loopCStrategy, loopQueue, loopBus);
    auto loopPacket = makeData(aFactory);
    for (int i = 0; i < 8 && loopPacket.ttl > 0U; ++i) {
        auto &service = (i % 2 == 0) ? loopB : loopC;
        assert(!service.processReceived(loopPacket));
        if (loopQueue.empty()) break;
        assert(loopQueue.dequeue(loopPacket));
    }
    assert(loopPacket.hopCount <= 2U);
    assert(loopEvents.drops > 0 || loopEvents.ttlFailures > 0);
    assert(loopQueue.size() <= constants::kPacketQueueCapacity);

    // Queue overflow is bounded: existing entries remain, new forwarding is dropped.
    queue::PacketQueue fullQueue;
    while (!fullQueue.full()) assert(fullQueue.enqueue(makeData(aFactory)));
    event::EventBus fullBus;
    Events fullEvents;
    assert(fullBus.subscribe(fullEvents));
    services::ForwardingService fullForward(bRoutes, bNode, bStrategy, fullQueue, fullBus);
    auto overflow = makeData(aFactory);
    assert(!fullForward.processReceived(overflow));
    assert(fullQueue.full() && fullEvents.drops == 1);

    // Broadcast remains local-only and produces no normal ACK.
    auto broadcast = makeData(aFactory);
    broadcast.destinationId = 0xFFU;
    assert(bAck.processReceivedData(broadcast));
    assert(bQueue.empty());
    assert(bForward.processReceived(broadcast));
    return 0;
}
