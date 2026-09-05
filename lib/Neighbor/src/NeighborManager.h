#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"
#include "Packet.h"

namespace jalari::neighbor
{
    enum class MetricState : std::uint8_t { Unknown, Valid, Stale };

    struct LinkMetric
    {
        MetricState state = MetricState::Unknown;
        float acknowledgementSuccessProbability = 0.0F;
        float etxCost = 0.0F;
        std::uint16_t observationCount = 0;
        std::uint32_t ageMs = 0;
    };

    struct Neighbor
    {
        std::uint8_t nodeId = 0;
        std::uint16_t lastSequenceNumber = 0;
        std::uint32_t lastSeenMs = 0;
        float averageRssi = 0.0F;
        float averageSnr = 0.0F;
        std::uint16_t packetSuccessCount = 0;
        std::uint16_t packetFailureCount = 0;
        std::uint16_t acknowledgementSuccessCount = 0;
        std::uint16_t acknowledgementFailureCount = 0;
        std::array<int, constants::kMaximumLinkMetricWindowSize> rssiSamples{};
        std::array<float, constants::kMaximumLinkMetricWindowSize> snrSamples{};
        std::size_t sampleCount = 0;
        std::size_t nextSample = 0;
        std::array<bool, constants::kAckMetricWindowSize> ackObservations{};
        std::size_t ackSampleCount = 0;
        std::size_t ackSuccessCount = 0;
        std::size_t nextAckSample = 0;
        std::uint32_t lastAckObservationMs = 0;
        bool active = false;

        float acknowledgementSuccessRate() const;
    };

    class NeighborManager
    {
    public:
        void observeHeartbeat(const protocol::Packet &packet, int rssi, float snr, std::uint32_t now,
                              std::size_t metricWindowSize);
        void recordPacketSuccess(std::uint8_t nodeId, int rssi, float snr, std::uint32_t now,
                                 std::size_t metricWindowSize);
        void recordAcknowledgementSuccess(std::uint8_t nodeId, std::uint32_t now = 0U);
        void recordAcknowledgementFailure(std::uint8_t nodeId, std::uint32_t now = 0U);
        bool expireOne(std::uint32_t now, std::uint32_t timeoutMs, Neighbor &outExpiredNeighbor);
        const Neighbor *find(std::uint8_t nodeId) const;
        bool getLinkMetric(std::uint8_t nodeId, std::uint32_t now, std::uint32_t staleTimeoutMs,
                           LinkMetric &outMetric) const;
        std::size_t count() const;

    private:
        Neighbor *findOrAllocate_(std::uint8_t nodeId);
        static void addMetricSample_(Neighbor &neighbor, int rssi, float snr, std::size_t metricWindowSize);
        static void addAcknowledgementObservation_(Neighbor &neighbor, bool success, std::uint32_t now);
        std::array<Neighbor, constants::kNeighborCapacity> neighbors_{};
    };
} // namespace jalari::neighbor
