#include <cassert>
#include <cstdint>

#include "AckManager.h"
#include "Config.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "NullForwardingStrategy.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "PacketQueue.h"
#include "Parser.h"
#include "Serializer.h"
#include "Validator.h"

namespace {
class Clock final : public jalari::hal::IClock {
public:
    std::uint32_t millis() const override { return now; }
    std::uint32_t now = 0;
};

class Listener final : public jalari::event::IEventListener {
public:
    void onEvent(const jalari::event::Event &event) override {
        if (event.type == jalari::event::Type::TransmissionRetryQueued) ++retries;
        if (event.type == jalari::event::Type::TransmissionFailed) ++failures;
        if (event.type == jalari::event::Type::DuplicateSuppressed) ++duplicates;
    }
    int retries = 0;
    int failures = 0;
    int duplicates = 0;
};

jalari::protocol::Packet dataPacket(jalari::protocol::PacketFactory &factory, std::uint8_t destination) {
    jalari::protocol::Packet packet;
    const std::uint8_t payload[] = {1, 2, 3};
    assert(factory.createData(destination, payload, sizeof(payload), packet));
    packet.nextHopId = destination; // N1 fixture models a direct link.
    return packet;
}
}

int main() {
    using namespace jalari;
    node::NodeManager node(config::kDefaultFirmwareConfig.node, node::DeviceRole::BoatNode);
    protocol::PacketFactory factory(node, config::kDefaultFirmwareConfig.network);
    queue::PacketQueue queue;
    event::EventBus bus;
    Listener listener;
    assert(bus.subscribe(listener));
    Clock clock;
    reliable::AckManager ack(config::kDefaultFirmwareConfig.reliability, node, factory, queue, clock);
    assert(ack.begin(bus));

    auto packet = dataPacket(factory, constants::kGatewayNodeId);
    std::array<std::uint8_t, constants::kMaxPacketSize> bytes{};
    std::size_t size = 0;
    assert(protocol::Serializer::serialize(packet, bytes, size));
    protocol::Packet parsed;
    assert(protocol::Parser::parse(bytes.data(), size, parsed));
    assert(protocol::Validator::isValid(parsed));
    bytes[size - 1] ^= 1U;
    assert(!protocol::Parser::parse(bytes.data(), size, parsed));

    bus.publish({event::Type::PacketSent, packet});
    clock.now = 1000;
    ack.update();
    assert(queue.size() == 1 && listener.retries == 1);
    queue.clear();
    protocol::Packet acknowledgement = packet;
    acknowledgement.type = protocol::PacketType::Acknowledgement;
    acknowledgement.sourceId = constants::kGatewayNodeId;
    acknowledgement.destinationId = node.id();
    const std::uint8_t ackPayload[] = {
        static_cast<std::uint8_t>(packet.sequenceNumber & 0xFFU),
        static_cast<std::uint8_t>(packet.sequenceNumber >> 8U)};
    assert(acknowledgement.setPayload(ackPayload, sizeof(ackPayload)));
    bus.publish({event::Type::PacketReceived, acknowledgement});
    assert(ack.pendingCount() == 0);

    auto mismatchPacket = dataPacket(factory, constants::kGatewayNodeId);
    bus.publish({event::Type::PacketSent, mismatchPacket});
    auto mismatchedAck = acknowledgement;
    mismatchedAck.payload[0] = static_cast<std::uint8_t>(mismatchPacket.sequenceNumber & 0xFFU);
    mismatchedAck.payload[1] = static_cast<std::uint8_t>(mismatchPacket.sequenceNumber >> 8U);
    mismatchedAck.sourceId = 0xFDU;
    bus.publish({event::Type::PacketReceived, mismatchedAck});
    assert(ack.pendingCount() == 1);
    mismatchedAck.sourceId = constants::kGatewayNodeId;
    bus.publish({event::Type::PacketReceived, mismatchedAck});
    assert(ack.pendingCount() == 0);

    auto failed = dataPacket(factory, constants::kGatewayNodeId);
    ack.onTransmissionFailed(failed);
    assert(queue.size() == 1);
    protocol::Packet retry;
    assert(queue.dequeue(retry));
    ack.onTransmissionFailed(retry);
    assert(queue.dequeue(retry));
    ack.onTransmissionFailed(retry);
    assert(queue.dequeue(retry));
    ack.onTransmissionFailed(retry);
    assert(queue.empty());
    assert(ack.pendingCount() == 0 && listener.failures == 1);

    auto incoming = dataPacket(factory, node.id());
    assert(ack.processReceivedData(incoming));
    assert(!ack.processReceivedData(incoming));
    assert(listener.duplicates == 1 && queue.size() == 2);
    queue.clear();
    clock.now = 1000U + constants::kDuplicateExpiryMs;
    assert(ack.processReceivedData(incoming));
    assert(!ack.processReceivedData(incoming));
    queue.clear();
    auto broadcast = dataPacket(factory, 0xFFU);
    assert(ack.processReceivedData(broadcast));
    assert(queue.empty());

    auto nonlocal = dataPacket(factory, 7U);
    forwarding::NullForwardingStrategy strategy;
    event::EventBus forwardingBus;
    services::ForwardingService forwarding(config::kDefaultFirmwareConfig.network, node, strategy, queue, forwardingBus);
    assert(!forwarding.processReceived(nonlocal));
    assert(forwarding.processReceived(incoming));
    protocol::Packet ttl = incoming;
    ttl.ttl = 1;
    assert(!ttl.prepareForForwarding(config::kDefaultFirmwareConfig.network.maximumHopCount));
    assert(ttl.hopCount == 0 && ttl.ttl == 0);

    while (!queue.full()) assert(queue.enqueue(incoming));
    auto queuedFailure = dataPacket(factory, constants::kGatewayNodeId);
    ack.onTransmissionFailed(queuedFailure);
    assert(ack.pendingCount() == 0);
    return 0;
}
