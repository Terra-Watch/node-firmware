#pragma once

#include <cstdint>

#include "Config.h"
#include "Packet.h"

namespace jalari::node { class NodeManager; }

namespace jalari::protocol
{
    class PacketFactory
    {
    public:
        PacketFactory(const node::NodeManager &node, const config::NetworkConfig &networkConfig);
        bool createHeartbeat(std::uint32_t uptimeMs, std::uint8_t batteryPct, std::uint8_t status, Packet &outPacket, std::uint32_t &outPacketId);
        bool createData(std::uint8_t destinationId, const std::uint8_t *payload, std::size_t payloadSize,
                        Packet &outPacket);
        bool createAcknowledgement(const Packet &receivedPacket, Packet &outPacket);
        bool createHeartbeatAck(std::uint32_t packetId, std::uint8_t targetNodeId, std::uint32_t timestamp, Packet &outPacket);
        bool createRouteAdvertisement(std::uint8_t destinationId, std::uint16_t routeMetric,
                                      std::uint8_t hopCount, std::uint16_t routeVersion, Packet &outPacket);

    private:
        void initializePacket_(PacketType type, std::uint8_t destinationId, Packet &outPacket);
        const node::NodeManager &node_;
        const config::NetworkConfig &networkConfig_;
        std::uint16_t nextSequenceNumber_ = 0;
        std::uint32_t nextPacketId_ = 1;
    };
} // namespace jalari::protocol
