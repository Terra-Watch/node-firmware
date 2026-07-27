#pragma once

#include <cstddef>
#include <cstdint>

namespace jalari::config
{

    struct RadioConfig
    {
        long frequencyHz;
        int sckPin;
        int misoPin;
        int mosiPin;
        int chipSelectPin;
        int resetPin;
        int dio0Pin;
    };

    struct NodeConfig
    {
        std::uint8_t nodeId;
    };

    struct ProtocolConfig
    {
        std::uint8_t preamble;
        std::uint8_t version;
        std::size_t maxPayloadSize;
        std::size_t maxRadioPacketSize;
    };

    struct PowerConfig
    {
        std::uint32_t heartbeatIntervalMs;
    };

    struct ReliabilityConfig
    {
        std::uint32_t acknowledgementTimeoutMs;
        std::uint8_t maxRetries;
    };

    struct NetworkConfig
    {
        std::uint8_t defaultTtl;
        std::uint8_t maximumHopCount;
        std::uint32_t neighborTimeoutMs;
        std::size_t linkMetricWindowSize;
    };

    struct FirmwareConfig
    {
        const RadioConfig radio;
        const NodeConfig node;
        const ProtocolConfig protocol;
        const PowerConfig power;
        const ReliabilityConfig reliability;
        const NetworkConfig network;
    };

    constexpr FirmwareConfig kDefaultFirmwareConfig{
        {433000000L, 12, 13, 11, 10, 9, 14},
        {1},
        {0xA5U, 3U, 48U, 255U},
        {2000U},
        {1000U, 3U},
        {8U, 16U, 30000U, 8U},
    };

} // namespace jalari::config
