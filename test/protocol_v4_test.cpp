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
    terrawatch::node::NodeManager node(terrawatch::config::kDefaultFirmwareConfig.node,
                                   terrawatch::node::DeviceRole::FieldNode);
    terrawatch::protocol::PacketFactory factory(node, terrawatch::config::kDefaultFirmwareConfig.network);

    terrawatch::protocol::Packet packet;
    std::array<std::uint8_t, terrawatch::constants::kMaxPayloadSize> payload{};
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::uint8_t>(i);
    assert(factory.createData(0xFEU, payload.data(), payload.size(), packet));
    packet.previousHopId = 0x02U;
    packet.nextHopId = 0x03U;
    packet.ttl = 7U;
    packet.hopCount = 1U;

    std::array<std::uint8_t, terrawatch::constants::kMaxPacketSize> bytes{};
    std::size_t size = 0;
    assert(terrawatch::protocol::Serializer::serialize(packet, bytes, size));
    assert(size == 61U);
    assert(bytes[5] == 0x02U && bytes[10] == 48U);
    assert(terrawatch::protocol::Validator::isValid(packet));

    terrawatch::protocol::Packet parsed;
    assert(terrawatch::protocol::Parser::parse(bytes.data(), size, parsed));
    assert(parsed.sourceId == packet.sourceId && parsed.destinationId == packet.destinationId);
    assert(parsed.previousHopId == 0x02U && parsed.sequenceNumber == packet.sequenceNumber);
    assert(parsed.nextHopId == 0U);
    assert(parsed.payloadSize == 48U && parsed.payload[47] == 47U);

    auto corrupted = bytes;
    corrupted[size - 1U] ^= 1U;
    assert(!terrawatch::protocol::Parser::parse(corrupted.data(), size, parsed));
    auto legacy = bytes;
    legacy[1] = 3U;
    assert(!terrawatch::protocol::Parser::parse(legacy.data(), size, parsed));

    terrawatch::protocol::Packet acknowledgement;
    assert(factory.createAcknowledgement(packet, acknowledgement));
    assert(acknowledgement.sourceId == node.id() && acknowledgement.destinationId == 0x02U);
    assert(acknowledgement.previousHopId == node.id());
    assert(acknowledgement.payloadSize == 2U);
    assert(acknowledgement.payload[0] == static_cast<std::uint8_t>(packet.sequenceNumber & 0xFFU));
    assert(acknowledgement.payload[1] == static_cast<std::uint8_t>(packet.sequenceNumber >> 8U));

    terrawatch::protocol::Packet broadcast;
    assert(factory.createData(0xFFU, payload.data(), 1U, broadcast));
    assert(broadcast.destinationId == 0xFFU && broadcast.previousHopId == node.id());
    return 0;
}
