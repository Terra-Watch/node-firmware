#include <cassert>
#include <cmath>
#include <cstdint>

#include "Constants.h"
#include "NeighborManager.h"
#include "Packet.h"

using namespace jalari;

static protocol::Packet heartbeatFrom(std::uint8_t nodeId) {
    protocol::Packet packet;
    packet.type = protocol::PacketType::Heartbeat;
    packet.sourceId = nodeId;
    packet.sequenceNumber = 1U;
    return packet;
}

static neighbor::LinkMetric metricFor(neighbor::NeighborManager &manager, std::uint8_t nodeId,
                                      std::uint32_t now = 0U) {
    neighbor::LinkMetric metric;
    assert(manager.getLinkMetric(nodeId, now, 30000U, metric));
    return metric;
}

int main() {
    neighbor::NeighborManager manager;
    manager.observeHeartbeat(heartbeatFrom(2U), -80, 7.0F, 0U, 8U);

    auto metric = metricFor(manager, 2U);
    assert(metric.state == neighbor::MetricState::Unknown);
    assert(metric.observationCount == 0U && metric.etxCost == 0.0F);

    manager.recordAcknowledgementSuccess(2U, 100U);
    manager.recordAcknowledgementSuccess(2U, 101U);
    metric = metricFor(manager, 2U, 101U);
    assert(metric.state == neighbor::MetricState::Valid);
    assert(metric.acknowledgementSuccessProbability == 1.0F && metric.etxCost == 1.0F);

    neighbor::NeighborManager eighty;
    eighty.observeHeartbeat(heartbeatFrom(3U), -75, 8.0F, 0U, 8U);
    for (int i = 0; i < 8; ++i) eighty.recordAcknowledgementSuccess(3U, static_cast<std::uint32_t>(i + 1));
    for (int i = 0; i < 2; ++i) eighty.recordAcknowledgementFailure(3U, static_cast<std::uint32_t>(i + 9));
    metric = metricFor(eighty, 3U, 10U);
    assert(metric.state == neighbor::MetricState::Valid);
    assert(std::fabs(metric.acknowledgementSuccessProbability - 0.8F) < 0.001F);
    assert(std::fabs(metric.etxCost - 1.25F) < 0.001F);

    neighbor::NeighborManager forty;
    forty.observeHeartbeat(heartbeatFrom(4U), -75, 8.0F, 0U, 8U);
    for (int i = 0; i < 4; ++i) forty.recordAcknowledgementSuccess(4U, static_cast<std::uint32_t>(i + 1));
    for (int i = 0; i < 6; ++i) forty.recordAcknowledgementFailure(4U, static_cast<std::uint32_t>(i + 5));
    metric = metricFor(forty, 4U, 10U);
    assert(std::fabs(metric.acknowledgementSuccessProbability - 0.4F) < 0.001F);
    assert(std::fabs(metric.etxCost - 2.5F) < 0.001F);

    neighbor::NeighborManager zero;
    zero.observeHeartbeat(heartbeatFrom(5U), -75, 8.0F, 0U, 8U);
    for (int i = 0; i < 4; ++i) zero.recordAcknowledgementFailure(5U, static_cast<std::uint32_t>(i + 1));
    metric = metricFor(zero, 5U, 4U);
    assert(metric.state == neighbor::MetricState::Valid);
    assert(metric.acknowledgementSuccessProbability == 0.0F && metric.etxCost == 10.0F);

    // A full window evicts old observations; old successes cannot dominate current failures.
    for (int i = 0; i < 16; ++i) zero.recordAcknowledgementFailure(5U, static_cast<std::uint32_t>(i + 100));
    metric = metricFor(zero, 5U, 115U);
    assert(metric.observationCount == constants::kAckMetricWindowSize);
    assert(metric.acknowledgementSuccessProbability == 0.0F && metric.etxCost == 10.0F);

    metric = metricFor(zero, 5U, 30116U);
    assert(metric.state == neighbor::MetricState::Stale);

    manager.observeHeartbeat(heartbeatFrom(6U), -60, 10.0F, 0U, 8U);
    manager.recordAcknowledgementFailure(6U, 10U);
    manager.recordAcknowledgementFailure(6U, 11U);
    const auto neighborTwo = metricFor(manager, 2U, 101U);
    const auto neighborSix = metricFor(manager, 6U, 11U);
    assert(neighborTwo.etxCost == 1.0F && neighborSix.etxCost == 10.0F);
    return 0;
}
