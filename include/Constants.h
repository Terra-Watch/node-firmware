#pragma once

#include <cstddef>

namespace jalari::constants
{

    constexpr std::size_t kPacketHeaderSize = 10;
    constexpr std::size_t kPacketCrcSize = 2;
    constexpr std::size_t kMaxPayloadSize = 48;
    constexpr std::size_t kMaxPacketSize = kPacketHeaderSize + kMaxPayloadSize + kPacketCrcSize;
    constexpr std::size_t kMaxRadioBufferSize = 255;
    constexpr std::size_t kPacketQueueCapacity = 8;
    constexpr std::size_t kEventBusMaxListeners = 8;
    constexpr std::size_t kNeighborCapacity = 16;
    constexpr std::size_t kDuplicatePacketCapacity = 32;
    constexpr std::size_t kPendingTransmissionCapacity = 8;
    constexpr std::size_t kAcknowledgementPayloadSize = 2;
    constexpr std::size_t kMaximumLinkMetricWindowSize = 8;

} // namespace jalari::constants
