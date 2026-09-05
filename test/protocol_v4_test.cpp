#include <array>
#include <cassert>
#include <cstdint>

#include "Config.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "Parser.h"
#include "Serializer.h"
#include "Validator.h"

int main() {
    jalari::node::NodeManager node(jalari::config::kDefaultFirmwareConfig.node,
                                   jalari::node::DeviceRole::BoatNode);
    jalari::protocol::PacketFactory factory(node, jalari::config::kDefaultFirmwareConfig.network);

    jalari::protocol::Packet packet;
    std::array<std::uint8_t, jalari::constants::kMaxPayloadSize> payload{};
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::uint8_t>(i);
    assert(factory.createData(0xFEU, payload.data(), payload.size(), packet));
    packet.previousHopId = 0x02U;
    packet.nextHopId = 0x03U;
    packet.ttl = 7U;
    packet.hopCount = 1U;

    std::array<std::uint8_t, jalari::constants::kMaxPacketSize> bytes{};
    std::size_t size = 0;
    assert(jalari::protocol::Serializer::serialize(packet, bytes, size));
    assert(size == 61U);
    assert(bytes[5] == 0x02U && bytes[10] == 48U);
    assert(jalari::protocol::Validator::isValid(packet));

    jalari::protocol::Packet parsed;
    assert(jalari::protocol::Parser::parse(bytes.data(), size, parsed));
    assert(parsed.sourceId == packet.sourceId && parsed.destinationId == packet.destinationId);
    assert(parsed.previousHopId == 0x02U && parsed.sequenceNumber == packet.sequenceNumber);
    assert(parsed.nextHopId == 0U);
    assert(parsed.payloadSize == 48U && parsed.payload[47] == 47U);

    auto corrupted = bytes;
    corrupted[size - 1U] ^= 1U;
    assert(!jalari::protocol::Parser::parse(corrupted.data(), size, parsed));
    auto legacy = bytes;
    legacy[1] = 3U;
    assert(!jalari::protocol::Parser::parse(legacy.data(), size, parsed));

    jalari::protocol::Packet acknowledgement;
    assert(factory.createAcknowledgement(packet, acknowledgement));
    assert(acknowledgement.sourceId == node.id() && acknowledgement.destinationId == 0x02U);
    assert(acknowledgement.previousHopId == node.id());
    assert(acknowledgement.payloadSize == 2U);
    assert(acknowledgement.payload[0] == static_cast<std::uint8_t>(packet.sequenceNumber & 0xFFU));
    assert(acknowledgement.payload[1] == static_cast<std::uint8_t>(packet.sequenceNumber >> 8U));

    jalari::protocol::Packet broadcast;
    assert(factory.createData(0xFFU, payload.data(), 1U, broadcast));
    assert(broadcast.destinationId == 0xFFU && broadcast.previousHopId == node.id());
    return 0;
}
