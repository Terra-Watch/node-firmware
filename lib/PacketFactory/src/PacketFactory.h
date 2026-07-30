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
        bool createHeartbeat(std::uint32_t uptimeMs, std::uint8_t batteryPct, std::uint8_t status, Packet &outPacket);
        bool createData(std::uint8_t destinationId, const std::uint8_t *payload, std::size_t payloadSize,
                        Packet &outPacket);
        bool createAcknowledgement(const Packet &receivedPacket, Packet &outPacket);

    private:
        void initializePacket_(PacketType type, std::uint8_t destinationId, Packet &outPacket);
        const node::NodeManager &node_;
        const config::NetworkConfig &networkConfig_;
        std::uint16_t nextSequenceNumber_ = 0;
    };
} // namespace jalari::protocol
