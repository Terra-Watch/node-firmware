#include "NeighborManager.h"

namespace jalari::neighbor
{
    float Neighbor::acknowledgementSuccessRate() const
    {
        const std::uint16_t attempts = acknowledgementSuccessCount + acknowledgementFailureCount;
        return attempts == 0U ? 0.0F : static_cast<float>(acknowledgementSuccessCount) / attempts;
    }

    void NeighborManager::observeHeartbeat(const protocol::Packet &packet, int rssi, float snr,
                                           std::uint32_t now, std::size_t metricWindowSize)
    {
        Neighbor *neighbor = findOrAllocate_(packet.sourceId);
        if (neighbor == nullptr) return;
        neighbor->lastSequenceNumber = packet.sequenceNumber;
        neighbor->lastSeenMs = now;
        neighbor->active = true;
        ++neighbor->packetSuccessCount;
        addMetricSample_(*neighbor, rssi, snr, metricWindowSize);
    }

    void NeighborManager::recordPacketSuccess(std::uint8_t nodeId, int rssi, float snr, std::uint32_t now,
                                              std::size_t metricWindowSize)
    {
        for (Neighbor &neighbor : neighbors_)
        {
            if (neighbor.active && neighbor.nodeId == nodeId)
            {
                ++neighbor.packetSuccessCount;
                neighbor.lastSeenMs = now;
                addMetricSample_(neighbor, rssi, snr, metricWindowSize);
                return;
            }
        }
    }

    void NeighborManager::recordAcknowledgementSuccess(std::uint8_t nodeId, std::uint32_t now)
    {
        for (Neighbor &neighbor : neighbors_)
            if (neighbor.active && neighbor.nodeId == nodeId)
            {
                ++neighbor.acknowledgementSuccessCount;
                addAcknowledgementObservation_(neighbor, true, now);
                return;
            }
    }

    void NeighborManager::recordAcknowledgementFailure(std::uint8_t nodeId, std::uint32_t now)
    {
        for (Neighbor &neighbor : neighbors_)
        {
            if (neighbor.active && neighbor.nodeId == nodeId)
            {
                ++neighbor.packetFailureCount;
                ++neighbor.acknowledgementFailureCount;
                addAcknowledgementObservation_(neighbor, false, now);
                return;
            }
        }
    }

    bool NeighborManager::expireOne(std::uint32_t now, std::uint32_t timeoutMs, Neighbor &outExpiredNeighbor)
    {
        for (Neighbor &neighbor : neighbors_)
        {
            if (neighbor.active && (now - neighbor.lastSeenMs) >= timeoutMs)
            {
                outExpiredNeighbor = neighbor;
                neighbor = {};
                return true;
            }
        }
        return false;
    }

    const Neighbor *NeighborManager::find(std::uint8_t nodeId) const
    {
        for (const Neighbor &neighbor : neighbors_) if (neighbor.active && neighbor.nodeId == nodeId) return &neighbor;
        return nullptr;
    }

    bool NeighborManager::getLinkMetric(std::uint8_t nodeId, std::uint32_t now,
                                        std::uint32_t staleTimeoutMs, LinkMetric &outMetric) const
    {
        const Neighbor *neighbor = find(nodeId);
        outMetric = {};
        if (neighbor == nullptr) return false;

        outMetric.observationCount = static_cast<std::uint16_t>(neighbor->ackSampleCount);
        if (neighbor->ackSampleCount < constants::kMinimumAckMetricObservations)
            return true;

        outMetric.ageMs = now - neighbor->lastAckObservationMs;
        if (outMetric.ageMs >= staleTimeoutMs)
        {
            outMetric.state = MetricState::Stale;
            return true;
        }

        outMetric.state = MetricState::Valid;
        outMetric.acknowledgementSuccessProbability =
            static_cast<float>(neighbor->ackSuccessCount) / static_cast<float>(neighbor->ackSampleCount);
        const float boundedProbability = outMetric.acknowledgementSuccessProbability < constants::kMinimumAckSuccessProbability
                                             ? constants::kMinimumAckSuccessProbability
                                             : outMetric.acknowledgementSuccessProbability;
        outMetric.etxCost = 1.0F / boundedProbability;
        return true;
    }

    std::size_t NeighborManager::count() const
    {
        std::size_t neighborCount = 0;
        for (const Neighbor &neighbor : neighbors_) if (neighbor.active) ++neighborCount;
        return neighborCount;
    }

    Neighbor *NeighborManager::findOrAllocate_(std::uint8_t nodeId)
    {
        for (Neighbor &neighbor : neighbors_) if (neighbor.active && neighbor.nodeId == nodeId) return &neighbor;
        for (Neighbor &neighbor : neighbors_)
        {
            if (!neighbor.active) { neighbor.nodeId = nodeId; neighbor.active = true; return &neighbor; }
        }
        return nullptr;
    }

    void NeighborManager::addMetricSample_(Neighbor &neighbor, int rssi, float snr, std::size_t metricWindowSize)
    {
        const std::size_t windowSize = metricWindowSize > constants::kMaximumLinkMetricWindowSize
                                           ? constants::kMaximumLinkMetricWindowSize : metricWindowSize;
        if (windowSize == 0U) return;
        neighbor.rssiSamples[neighbor.nextSample] = rssi;
        neighbor.snrSamples[neighbor.nextSample] = snr;
        neighbor.nextSample = (neighbor.nextSample + 1U) % windowSize;
        if (neighbor.sampleCount < windowSize) ++neighbor.sampleCount;

        float rssiTotal = 0.0F;
        float snrTotal = 0.0F;
        for (std::size_t index = 0; index < neighbor.sampleCount; ++index)
        {
            rssiTotal += neighbor.rssiSamples[index];
            snrTotal += neighbor.snrSamples[index];
        }
        neighbor.averageRssi = rssiTotal / neighbor.sampleCount;
        neighbor.averageSnr = snrTotal / neighbor.sampleCount;
    }

    void NeighborManager::addAcknowledgementObservation_(Neighbor &neighbor, bool success, std::uint32_t now)
    {
        if (neighbor.ackSampleCount == constants::kAckMetricWindowSize && neighbor.ackObservations[neighbor.nextAckSample])
            --neighbor.ackSuccessCount;
        else if (neighbor.ackSampleCount < constants::kAckMetricWindowSize)
            ++neighbor.ackSampleCount;

        neighbor.ackObservations[neighbor.nextAckSample] = success;
        if (success) ++neighbor.ackSuccessCount;
        neighbor.nextAckSample = (neighbor.nextAckSample + 1U) % constants::kAckMetricWindowSize;
        neighbor.lastAckObservationMs = now;
    }
} // namespace jalari::neighbor
