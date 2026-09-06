#include "NeighborService.h"

#include "IClock.h"

namespace jalari::services
{
    NeighborService::NeighborService(const config::NetworkConfig &config, neighbor::NeighborManager &neighborManager,
                                     hal::IClock &clock)
        : config_(config), neighborManager_(neighborManager), clock_(clock) {}

    bool NeighborService::begin(event::EventBus &eventBus)
    {
        eventBus_ = &eventBus;
        return eventBus.subscribe(*this);
    }

    void NeighborService::update()
    {
        neighbor::Neighbor expiredNeighbor;
        while (neighborManager_.expireOne(clock_.millis(), config_.neighborTimeoutMs, expiredNeighbor))
        {
            if (eventBus_ == nullptr) continue;
            event::Event event(event::Type::NeighborExpired);
            event.nodeId = expiredNeighbor.nodeId;
            eventBus_->publish(event);
        }
    }

    void NeighborService::onEvent(const event::Event &event)
    {
        if (event.type == event::Type::PacketReceived)
        {
            if (event.packet.type == protocol::PacketType::Heartbeat)
            {
                neighborManager_.observeHeartbeat(event.packet, event.rssi, event.snr, clock_.millis(), config_.linkMetricWindowSize);
                if (eventBus_ != nullptr) eventBus_->publish({event::Type::HeartbeatReceived, event.packet, event.rssi, event.snr});
                publishMetricsUpdated_(event.packet.sourceId);
            }
            else if (neighborManager_.find(event.packet.sourceId) != nullptr)
            {
                neighborManager_.recordPacketSuccess(event.packet.sourceId, event.rssi, event.snr, clock_.millis(), config_.linkMetricWindowSize);
                publishMetricsUpdated_(event.packet.sourceId);
            }
        }
        else if (event.type == event::Type::AcknowledgementReceived)
        {
            neighborManager_.recordAcknowledgementSuccess(event.packet.sourceId, clock_.millis());
            publishMetricsUpdated_(event.packet.sourceId);
        }
        else if (event.type == event::Type::AcknowledgementFailed)
        {
            neighborManager_.recordAcknowledgementFailure(event.packet.destinationId, clock_.millis());
            publishMetricsUpdated_(event.packet.destinationId);
        }
    }

    void NeighborService::publishMetricsUpdated_(std::uint8_t nodeId)
    {
        if (eventBus_ == nullptr) return;
        event::Event event(event::Type::LinkMetricsUpdated);
        event.nodeId = nodeId;
        eventBus_->publish(event);
    }
} // namespace jalari::services
