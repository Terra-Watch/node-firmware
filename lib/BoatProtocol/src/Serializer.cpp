#include "Serializer.h"

#include "Config.h"
#include "Crc16.h"

namespace jalari::protocol
{

    bool Serializer::serialize(const Packet &packet,
                               std::array<std::uint8_t, constants::kMaxPacketSize> &outBuffer,
                               std::size_t &outSize)
    {
        if (packet.payloadSize > constants::kMaxPayloadSize)
        {
            return false;
        }

        outBuffer.fill(0);

        outBuffer[0] = config::kDefaultFirmwareConfig.protocol.preamble;
        outBuffer[1] = config::kDefaultFirmwareConfig.protocol.version;
        outBuffer[2] = static_cast<std::uint8_t>(packet.type);
        outBuffer[3] = packet.sourceId;
        outBuffer[4] = packet.destinationId;
        outBuffer[5] = packet.previousHopId;
        outBuffer[6] = static_cast<std::uint8_t>(packet.sequenceNumber & 0xFFU);
        outBuffer[7] = static_cast<std::uint8_t>((packet.sequenceNumber >> 8U) & 0xFFU);
        outBuffer[8] = packet.ttl;
        outBuffer[9] = packet.hopCount;
        outBuffer[10] = static_cast<std::uint8_t>(packet.payloadSize);

        for (std::size_t i = 0; i < packet.payloadSize; ++i)
        {
            outBuffer[constants::kPacketHeaderSize + i] = packet.payload[i];
        }

        const std::size_t packetNoCrcSize = constants::kPacketHeaderSize + packet.payloadSize;
        const std::uint16_t crc = utils::Crc16::computeCcitt(outBuffer.data(), packetNoCrcSize);

        outBuffer[packetNoCrcSize] = static_cast<std::uint8_t>(crc & 0xFFU);
        outBuffer[packetNoCrcSize + 1] = static_cast<std::uint8_t>((crc >> 8) & 0xFFU);

        outSize = packetNoCrcSize + constants::kPacketCrcSize;
        return true;
    }

} // namespace jalari::protocol
