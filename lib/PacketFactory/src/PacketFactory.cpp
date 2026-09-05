#include "PacketFactory.h"
#include "NodeManager.h"
#include "Version.h"
#include <cstdio>

namespace jalari::protocol
{
    PacketFactory::PacketFactory(const node::NodeManager &node, const config::NetworkConfig &networkConfig)
        : node_(node), networkConfig_(networkConfig) {}

    bool PacketFactory::createHeartbeat(std::uint32_t uptimeMs, std::uint8_t batteryPct, std::uint8_t status, Packet &outPacket, std::uint32_t &outPacketId)
    {
        std::uint8_t fwMajor = 0;
        std::uint8_t fwMinor = 0;
        std::uint8_t fwPatch = 0;

        unsigned int maj = 0, min = 0, pat = 0;
        if (std::sscanf(version::kFirmwareVersion, "%u.%u.%u", &maj, &min, &pat) == 3)
        {
            fwMajor = static_cast<std::uint8_t>(maj);
            fwMinor = static_cast<std::uint8_t>(min);
            fwPatch = static_cast<std::uint8_t>(pat);
        }

        outPacketId = nextPacketId_++;

        std::uint8_t payload[13];
        payload[0] = static_cast<std::uint8_t>(outPacketId & 0xFFU);
        payload[1] = static_cast<std::uint8_t>((outPacketId >> 8U) & 0xFFU);
        payload[2] = static_cast<std::uint8_t>((outPacketId >> 16U) & 0xFFU);
        payload[3] = static_cast<std::uint8_t>((outPacketId >> 24U) & 0xFFU);

        payload[4] = static_cast<std::uint8_t>(uptimeMs & 0xFFU);
        payload[5] = static_cast<std::uint8_t>((uptimeMs >> 8U) & 0xFFU);
        payload[6] = static_cast<std::uint8_t>((uptimeMs >> 16U) & 0xFFU);
        payload[7] = static_cast<std::uint8_t>((uptimeMs >> 24U) & 0xFFU);

        payload[8] = fwMajor;
        payload[9] = fwMinor;
        payload[10] = fwPatch;
        payload[11] = batteryPct;
        payload[12] = status;

        initializePacket_(PacketType::Heartbeat, 0xFFU, outPacket);
        return outPacket.setPayload(payload, sizeof(payload));
    }

    bool PacketFactory::createData(std::uint8_t destinationId, const std::uint8_t *payload,
                                   std::size_t payloadSize, Packet &outPacket)
    {
        if (payload == nullptr && payloadSize != 0U) return false;
        initializePacket_(PacketType::Data, destinationId, outPacket);
        for (std::size_t i = 0; i < networkConfig_.staticRouteCount && i < networkConfig_.staticRoutes.size(); ++i)
        {
            if (networkConfig_.staticRoutes[i].destinationId == destinationId)
            {
                outPacket.nextHopId = networkConfig_.staticRoutes[i].nextHopId;
                break;
            }
        }
        return outPacket.setPayload(payload, payloadSize);
    }

    bool PacketFactory::createAcknowledgement(const Packet &receivedPacket, Packet &outPacket)
    {
        const std::uint8_t payload[] = {
            static_cast<std::uint8_t>(receivedPacket.sequenceNumber & 0xFFU),
            static_cast<std::uint8_t>((receivedPacket.sequenceNumber >> 8U) & 0xFFU),
        };
        initializePacket_(PacketType::Acknowledgement, receivedPacket.previousHopId, outPacket);
        return outPacket.setPayload(payload, sizeof(payload));
    }

    bool PacketFactory::createHeartbeatAck(std::uint32_t packetId, std::uint8_t targetNodeId, std::uint32_t timestamp, Packet &outPacket)
    {
        std::uint8_t payload[9];
        payload[0] = static_cast<std::uint8_t>(packetId & 0xFFU);
        payload[1] = static_cast<std::uint8_t>((packetId >> 8U) & 0xFFU);
        payload[2] = static_cast<std::uint8_t>((packetId >> 16U) & 0xFFU);
        payload[3] = static_cast<std::uint8_t>((packetId >> 24U) & 0xFFU);
        payload[4] = targetNodeId;
        payload[5] = static_cast<std::uint8_t>(timestamp & 0xFFU);
        payload[6] = static_cast<std::uint8_t>((timestamp >> 8U) & 0xFFU);
        payload[7] = static_cast<std::uint8_t>((timestamp >> 16U) & 0xFFU);
        payload[8] = static_cast<std::uint8_t>((timestamp >> 24U) & 0xFFU);

        initializePacket_(PacketType::HeartbeatAck, targetNodeId, outPacket);
        return outPacket.setPayload(payload, sizeof(payload));
    }

    bool PacketFactory::createRouteAdvertisement(std::uint8_t destinationId, std::uint16_t routeMetric,
                                                 std::uint8_t hopCount, std::uint16_t routeVersion, Packet &outPacket)
    {
        if (destinationId == 0U || destinationId == 0xFFU || routeMetric == 0U || hopCount == 0U) return false;
        const std::uint8_t payload[] = {
            destinationId,
            static_cast<std::uint8_t>(routeMetric & 0xFFU),
            static_cast<std::uint8_t>((routeMetric >> 8U) & 0xFFU),
            hopCount,
            static_cast<std::uint8_t>(routeVersion & 0xFFU),
            static_cast<std::uint8_t>((routeVersion >> 8U) & 0xFFU),
        };
        initializePacket_(PacketType::RouteAdvertisement, 0xFFU, outPacket);
        return outPacket.setPayload(payload, sizeof(payload));
    }

    void PacketFactory::initializePacket_(PacketType type, std::uint8_t destinationId, Packet &outPacket)
    {
        outPacket = {};
        outPacket.type = type;
        outPacket.sourceId = node_.id();
        outPacket.destinationId = destinationId;
        outPacket.previousHopId = node_.id();
        outPacket.sequenceNumber = nextSequenceNumber_++;
        outPacket.ttl = networkConfig_.defaultTtl;
        outPacket.hopCount = 0;
    }
} // namespace jalari::protocol
