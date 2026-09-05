#pragma once

#include <cstddef>

namespace jalari::constants
{

    constexpr std::size_t kPacketHeaderSize = 11;
    constexpr std::size_t kPacketCrcSize = 2;
    constexpr std::size_t kMaxPayloadSize = 48;
    constexpr std::size_t kMaxPacketSize = kPacketHeaderSize + kMaxPayloadSize + kPacketCrcSize;
    constexpr std::size_t kMaxRadioBufferSize = 255;
    constexpr std::size_t kPacketQueueCapacity = 8;
    constexpr std::size_t kEventBusMaxListeners = 8;
    constexpr std::size_t kNeighborCapacity = 16;
    constexpr std::size_t kDuplicatePacketCapacity = 32;
    constexpr std::size_t kPendingTransmissionCapacity = 8;
    constexpr std::uint32_t kDuplicateExpiryMs = 10000U;
    constexpr std::size_t kAcknowledgementPayloadSize = 2;
    constexpr std::uint8_t kGatewayNodeId = 0xFEU;
    constexpr std::size_t kMaximumLinkMetricWindowSize = 8;
    constexpr std::size_t kAckMetricWindowSize = 16;
    constexpr std::size_t kMinimumAckMetricObservations = 2;
    constexpr float kMinimumAckSuccessProbability = 0.1F;
    constexpr std::size_t kRouteTableCapacity = 16;
    constexpr std::uint32_t kDefaultRouteLifetimeMs = 60000U;
    constexpr std::uint16_t kRouteMetricScale = 100U;
    constexpr std::size_t kRouteAdvertisementPayloadSize = 6U;
    constexpr std::size_t kRouteAdvertisementDuplicateCapacity = 32U;
    constexpr std::uint32_t kRouteAdvertisementIntervalMs = 15000U;
    constexpr std::size_t kDtnQueueCapacity = 8U;
    constexpr std::uint32_t kDtnQueueLifetimeMs = 300000U;
    constexpr std::size_t kDtnProcessingBatchSize = 2U;
    constexpr std::size_t kTestCommandLineCapacity = 96U;
    constexpr std::uint16_t kTestDataCountMaximum = 100U;

} // namespace jalari::constants
