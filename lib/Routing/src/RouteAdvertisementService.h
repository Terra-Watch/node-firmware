#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "EventBus.h"
#include "IClock.h"
#include "NeighborManager.h"
#include "PacketFactory.h"
#include "PacketQueue.h"
#include "RouteTable.h"

namespace jalari::routing
{
    class RouteAdvertisementService final : public event::IEventListener
    {
    public:
        RouteAdvertisementService(std::uint8_t localNodeId, RouteTable &routeTable,
                                   neighbor::NeighborManager &neighborManager,
                                   protocol::PacketFactory &packetFactory, queue::PacketQueue &packetQueue,
                                   hal::IClock &clock, std::uint32_t neighborMetricStaleMs,
                                   std::uint32_t advertisementIntervalMs = constants::kRouteAdvertisementIntervalMs);

        bool begin(event::EventBus &eventBus);
        void update();
        bool processReceived(const protocol::Packet &packet, std::uint32_t nowMs);
        bool advertiseRoute(std::uint8_t destinationId, std::uint32_t nowMs);
        void onEvent(const event::Event &event) override;

    private:
        struct DuplicateKey
        {
            std::uint8_t sourceId = 0;
            std::uint8_t destinationId = 0;
            std::uint16_t routeVersion = 0;
            std::uint32_t seenMs = 0;
            bool active = false;
        };
        struct AdvertisementState
        {
            std::uint8_t destinationId = 0;
            std::uint32_t lastSentMs = 0;
            bool active = false;
        };

        bool rememberDuplicate_(std::uint8_t sourceId, std::uint8_t destinationId,
                                std::uint16_t routeVersion, std::uint32_t nowMs);
        bool parse_(const protocol::Packet &packet, std::uint8_t &destinationId,
                    std::uint16_t &metric, std::uint8_t &hopCount, std::uint16_t &version) const;

        const std::uint8_t localNodeId_;
        RouteTable &routeTable_;
        neighbor::NeighborManager &neighborManager_;
        protocol::PacketFactory &packetFactory_;
        queue::PacketQueue &packetQueue_;
        hal::IClock &clock_;
        const std::uint32_t neighborMetricStaleMs_;
        const std::uint32_t advertisementIntervalMs_;
        event::EventBus *eventBus_ = nullptr;
        std::array<DuplicateKey, constants::kRouteAdvertisementDuplicateCapacity> duplicates_{};
        std::array<AdvertisementState, constants::kRouteTableCapacity> advertisements_{};
        std::size_t nextDuplicate_ = 0;
    };
} // namespace jalari::routing
