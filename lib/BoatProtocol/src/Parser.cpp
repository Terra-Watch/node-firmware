#include "Parser.h"

#include "Config.h"
#include "Constants.h"
#include "Crc16.h"

namespace jalari::protocol
{

    bool Parser::parse(const std::uint8_t *data, std::size_t size, Packet &outPacket)
    {
        if (data == nullptr)
        {
            return false;
        }

        if (size < constants::kPacketHeaderSize + constants::kPacketCrcSize)
        {
            return false;
        }

        if (data[0] != config::kDefaultFirmwareConfig.protocol.preamble)
        {
            return false;
        }

        if (data[1] != config::kDefaultFirmwareConfig.protocol.version)
        {
            return false;
        }

        const std::size_t payloadSize = data[10];
        const std::size_t expectedSize = constants::kPacketHeaderSize + payloadSize + constants::kPacketCrcSize;

        if (payloadSize > constants::kMaxPayloadSize || size != expectedSize)
        {
            return false;
        }

        const std::uint16_t expectedCrc = static_cast<std::uint16_t>(data[expectedSize - 2]) |
                                          (static_cast<std::uint16_t>(data[expectedSize - 1]) << 8);
        const std::uint16_t actualCrc = utils::Crc16::computeCcitt(data, expectedSize - constants::kPacketCrcSize);

        if (expectedCrc != actualCrc)
        {
            return false;
        }

        outPacket.type = static_cast<PacketType>(data[2]);
        outPacket.sourceId = data[3];
        outPacket.destinationId = data[4];
        outPacket.previousHopId = data[5];
        outPacket.sequenceNumber = static_cast<std::uint16_t>(data[6]) |
                                   (static_cast<std::uint16_t>(data[7]) << 8U);
        outPacket.ttl = data[8];
        outPacket.hopCount = data[9];
        outPacket.payloadSize = payloadSize;

        for (std::size_t i = 0; i < payloadSize; ++i)
        {
            outPacket.payload[i] = data[constants::kPacketHeaderSize + i];
        }

        return true;
    }

} // namespace jalari::protocol
