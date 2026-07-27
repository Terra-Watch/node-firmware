#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"

namespace jalari::protocol
{

    enum class PacketType : std::uint8_t
    {
        Unknown = 0,
        Data = 1,
        Test = Data,
        Heartbeat = 2,
        Acknowledgement = 3,
    };

    struct Packet
    {
        PacketType type = PacketType::Unknown;
        std::uint8_t sourceId = 0;
        std::uint8_t destinationId = 0;
        std::uint16_t sequenceNumber = 0;
        std::uint8_t ttl = 0;
        std::uint8_t hopCount = 0;
        std::array<std::uint8_t, constants::kMaxPayloadSize> payload{};
        std::size_t payloadSize = 0;

        bool setPayload(const std::uint8_t *data, std::size_t size);
        bool prepareForForwarding(std::uint8_t maximumHopCount);
    };

} // namespace jalari::protocol
