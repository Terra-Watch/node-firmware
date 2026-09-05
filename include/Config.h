#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

namespace jalari::config
{

#ifndef JALARI_NODE_ID
#define JALARI_NODE_ID 1U
#endif
#ifndef JALARI_TEST_MODE
#define JALARI_TEST_MODE 0
#endif

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
        std::uint32_t telemetryIntervalMs;
    };

    struct SensorConfig
    {
        int i2cSdaPin;
        int i2cSclPin;
    };

    struct ReliabilityConfig
    {
        std::uint32_t acknowledgementTimeoutMs;
        std::uint8_t maxRetries;
    };

    struct NetworkConfig
    {
        struct StaticRoute
        {
            constexpr StaticRoute(std::uint8_t destination = 0U, std::uint8_t nextHop = 0U)
                : destinationId(destination), nextHopId(nextHop) {}
            std::uint8_t destinationId = 0;
            std::uint8_t nextHopId = 0;
        };
        static constexpr std::size_t kStaticRouteCapacity = 8U;
        constexpr NetworkConfig(std::uint8_t ttl = 0U, std::uint8_t maxHops = 0U,
                                 std::uint32_t neighborTimeout = 0U, std::size_t metricWindow = 0U,
                                 std::uint8_t routeDestination = 0U, std::uint8_t routeNextHop = 0U)
            : defaultTtl(ttl), maximumHopCount(maxHops), neighborTimeoutMs(neighborTimeout),
              linkMetricWindowSize(metricWindow),
              staticRoutes{StaticRoute(routeDestination, routeNextHop)},
              staticRouteCount((routeDestination != 0U && routeNextHop != 0U) ? 1U : 0U) {}
        std::uint8_t defaultTtl;
        std::uint8_t maximumHopCount;
        std::uint32_t neighborTimeoutMs;
        std::size_t linkMetricWindowSize;
        std::array<StaticRoute, kStaticRouteCapacity> staticRoutes{};
        std::size_t staticRouteCount = 0;
    };

    struct FirmwareConfig
    {
        const RadioConfig radio;
        const NodeConfig node;
        const ProtocolConfig protocol;
        const PowerConfig power;
        const SensorConfig sensors;
        const ReliabilityConfig reliability;
        const NetworkConfig network;
    };

    constexpr NetworkConfig kDefaultNetworkConfig(8U, 16U, 30000U, 8U, 0xFEU, 0x02U);

    constexpr FirmwareConfig kDefaultFirmwareConfig{
        {433000000L, 12, 13, 11, 10, 9, 14},
        {static_cast<std::uint8_t>(JALARI_NODE_ID)},
        {0xA5U, 4U, 48U, 255U},
        {2000U, 5000U},
        {21, 18},
        {1000U, 3U},
        kDefaultNetworkConfig,
    };

} // namespace jalari::config
