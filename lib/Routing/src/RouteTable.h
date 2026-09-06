#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"
#include "NeighborManager.h"

namespace jalari::routing
{
    struct RouteEntry
    {
        constexpr RouteEntry(std::uint8_t destination = 0U, std::uint8_t nextHop = 0U,
                             std::uint16_t routeMetric = 0U, std::uint8_t hops = 0U,
                             std::uint32_t updated = 0U, std::uint16_t version = 0U,
                             bool routeValid = false)
            : destinationId(destination), nextHopId(nextHop), metric(routeMetric), hopCount(hops),
              lastUpdatedMs(updated), routeVersion(version), valid(routeValid) {}
        std::uint8_t destinationId = 0;
        std::uint8_t nextHopId = 0;
        std::uint16_t metric = 0; // Route cost multiplied by kRouteMetricScale.
        std::uint8_t hopCount = 0;
        std::uint32_t lastUpdatedMs = 0;
        std::uint16_t routeVersion = 0;
        bool valid = false;
    };

    class RouteTable
    {
    public:
        explicit RouteTable(std::uint8_t localNodeId,
                            std::uint32_t routeLifetimeMs = constants::kDefaultRouteLifetimeMs);

        bool updateRoute(std::uint8_t destinationId, std::uint8_t nextHopId, std::uint16_t metric,
                         std::uint8_t hopCount, std::uint16_t routeVersion, std::uint32_t nowMs);
        bool updateRoute(std::uint8_t destinationId, std::uint8_t nextHopId,
                         const neighbor::LinkMetric &linkMetric, std::uint8_t hopCount,
                         std::uint16_t routeVersion, std::uint32_t nowMs);
        bool lookup(std::uint8_t destinationId, std::uint32_t nowMs, RouteEntry &outRoute) const;
        bool getValidAt(std::size_t index, std::uint32_t nowMs, RouteEntry &outRoute) const;
        bool find(std::uint8_t destinationId, RouteEntry &outRoute) const;
        bool invalidateRoute(std::uint8_t destinationId);
        bool invalidateRoute(std::uint8_t destinationId, std::uint8_t expectedNextHopId);
        bool removeRoute(std::uint8_t destinationId);
        std::size_t expireStale(std::uint32_t nowMs);
        void clear();
        std::size_t count() const;
        std::size_t validCount(std::uint32_t nowMs) const;

    private:
        static bool newerVersion_(std::uint16_t candidate, std::uint16_t current);
        static bool betterRoute_(const RouteEntry &candidate, const RouteEntry &current);
        bool isUsable_(const RouteEntry &route, std::uint32_t nowMs) const;
        std::size_t findIndex_(std::uint8_t destinationId) const;
        std::size_t selectVictim_(const RouteEntry &candidate, std::uint32_t nowMs) const;

        const std::uint8_t localNodeId_;
        const std::uint32_t routeLifetimeMs_;
        std::array<RouteEntry, constants::kRouteTableCapacity> routes_{};
    };
} // namespace jalari::routing
