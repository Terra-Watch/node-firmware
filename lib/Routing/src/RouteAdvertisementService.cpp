#include "RouteAdvertisementService.h"

#include <cmath>

namespace jalari::routing
{
    RouteAdvertisementService::RouteAdvertisementService(
        std::uint8_t localNodeId, RouteTable &routeTable, neighbor::NeighborManager &neighborManager,
        protocol::PacketFactory &packetFactory, queue::PacketQueue &packetQueue, hal::IClock &clock,
        std::uint32_t neighborMetricStaleMs, std::uint32_t advertisementIntervalMs)
        : localNodeId_(localNodeId), routeTable_(routeTable), neighborManager_(neighborManager),
          packetFactory_(packetFactory), packetQueue_(packetQueue), clock_(clock),
          neighborMetricStaleMs_(neighborMetricStaleMs), advertisementIntervalMs_(advertisementIntervalMs) {}

    bool RouteAdvertisementService::begin(event::EventBus &eventBus)
    {
        eventBus_ = &eventBus;
        return eventBus.subscribe(*this);
    }

    void RouteAdvertisementService::update()
    {
        const std::uint32_t nowMs = clock_.millis();
        for (std::size_t index = 0; index < constants::kRouteTableCapacity; ++index)
        {
            RouteEntry route;
            if (routeTable_.getValidAt(index, nowMs, route))
                (void)advertiseRoute(route.destinationId, nowMs);
        }
    }

    bool RouteAdvertisementService::processReceived(const protocol::Packet &packet, std::uint32_t nowMs)
    {
        std::uint8_t destinationId = 0;
        std::uint8_t advertisedHopCount = 0;
        std::uint16_t advertisedMetric = 0;
        std::uint16_t version = 0;
        if (packet.type != protocol::PacketType::RouteAdvertisement || packet.sourceId == localNodeId_ ||
            packet.previousHopId == 0U || !parse_(packet, destinationId, advertisedMetric, advertisedHopCount, version))
            return false;

        if (rememberDuplicate_(packet.sourceId, destinationId, version, nowMs))
        {
            if (eventBus_ != nullptr) eventBus_->publish({event::Type::RouteAdvertisementSuppressed, packet});
            return false;
        }

        neighbor::LinkMetric localMetric;
        if (!neighborManager_.getLinkMetric(packet.previousHopId, nowMs, neighborMetricStaleMs_, localMetric) ||
            localMetric.state != neighbor::MetricState::Valid || !std::isfinite(localMetric.etxCost))
        {
            if (eventBus_ != nullptr) eventBus_->publish({event::Type::RouteAdvertisementRejected, packet});
            return false;
        }

        const float totalCost = localMetric.etxCost * static_cast<float>(constants::kRouteMetricScale) +
                                static_cast<float>(advertisedMetric);
        if (totalCost > 65535.0F || advertisedHopCount == 255U)
        {
            if (eventBus_ != nullptr) eventBus_->publish({event::Type::RouteAdvertisementRejected, packet});
            return false;
        }

        RouteEntry previousRoute;
        const bool wasUsable = routeTable_.lookup(destinationId, nowMs, previousRoute);
        const bool updated = routeTable_.updateRoute(
            destinationId, packet.previousHopId, static_cast<std::uint16_t>(totalCost + 0.5F),
            static_cast<std::uint8_t>(advertisedHopCount + 1U), version, nowMs);
        if (updated && eventBus_ != nullptr)
        {
            eventBus_->publish({event::Type::RouteAdvertisementReceived, packet});
            if (!wasUsable) eventBus_->publish({event::Type::RouteRecovered, packet});
        }
        return updated;
    }

    bool RouteAdvertisementService::advertiseRoute(std::uint8_t destinationId, std::uint32_t nowMs)
    {
        RouteEntry route;
        if (!routeTable_.lookup(destinationId, nowMs, route)) return false;

        AdvertisementState *state = nullptr;
        for (auto &entry : advertisements_)
            if (entry.active && entry.destinationId == destinationId) { state = &entry; break; }
        if (state != nullptr && (nowMs - state->lastSentMs) < advertisementIntervalMs_) return false;
        if (state == nullptr)
            for (auto &entry : advertisements_)
                if (!entry.active) { state = &entry; entry.active = true; entry.destinationId = destinationId; break; }
        if (state == nullptr) return false;

        protocol::Packet advertisement;
        if (!packetFactory_.createRouteAdvertisement(destinationId, route.metric, route.hopCount,
                                                     route.routeVersion, advertisement) ||
            !packetQueue_.enqueue(advertisement)) return false;
        state->lastSentMs = nowMs;
        return true;
    }

    void RouteAdvertisementService::onEvent(const event::Event &event)
    {
        if (event.type == event::Type::PacketReceived &&
            event.packet.type == protocol::PacketType::RouteAdvertisement)
            (void)processReceived(event.packet, clock_.millis());
    }

    bool RouteAdvertisementService::rememberDuplicate_(std::uint8_t sourceId, std::uint8_t destinationId,
                                                       std::uint16_t routeVersion, std::uint32_t nowMs)
    {
        for (auto &entry : duplicates_)
        {
            if (entry.active && (nowMs - entry.seenMs) >= (advertisementIntervalMs_ * 2U)) entry.active = false;
            if (entry.active && entry.sourceId == sourceId && entry.destinationId == destinationId &&
                entry.routeVersion == routeVersion) return true;
        }
        auto &entry = duplicates_[nextDuplicate_];
        entry.sourceId = sourceId;
        entry.destinationId = destinationId;
        entry.routeVersion = routeVersion;
        entry.seenMs = nowMs;
        entry.active = true;
        nextDuplicate_ = (nextDuplicate_ + 1U) % duplicates_.size();
        return false;
    }

    bool RouteAdvertisementService::parse_(const protocol::Packet &packet, std::uint8_t &destinationId,
                                           std::uint16_t &metric, std::uint8_t &hopCount,
                                           std::uint16_t &version) const
    {
        if (packet.destinationId != 0xFFU || packet.payloadSize != constants::kRouteAdvertisementPayloadSize)
            return false;
        destinationId = packet.payload[0];
        metric = static_cast<std::uint16_t>(packet.payload[1]) |
                 (static_cast<std::uint16_t>(packet.payload[2]) << 8U);
        hopCount = packet.payload[3];
        version = static_cast<std::uint16_t>(packet.payload[4]) |
                  (static_cast<std::uint16_t>(packet.payload[5]) << 8U);
        return destinationId != 0U && destinationId != 0xFFU && destinationId != localNodeId_ &&
               metric != 0U && hopCount != 0U;
    }
} // namespace jalari::routing
