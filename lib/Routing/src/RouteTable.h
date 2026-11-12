#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"
#include "NeighborManager.h"

namespace terrawatch::routing
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
        bool learned = false; // reverse route learned from data heard passing by (towards a field node); never advertised
        // Second-best parent for the same destination (a different neighbour, same route version). Zero next hop = none.
        std::uint8_t backupNextHopId = 0;
        std::uint16_t backupMetric = 0;
        std::uint8_t backupHopCount = 0;
        std::uint32_t backupUpdatedMs = 0;
        // What the next hop itself advertised (0 = not known). A backup is loop-free only if its advertised cost is below our primary's total cost.
        std::uint16_t advertisedMetric = 0;
        std::uint16_t backupAdvertisedMetric = 0;
    };

    class RouteTable
    {
    public:
        explicit RouteTable(std::uint8_t localNodeId,
                            std::uint32_t routeLifetimeMs = constants::kDefaultRouteLifetimeMs);

        bool updateRoute(std::uint8_t destinationId, std::uint8_t nextHopId, std::uint16_t metric,
                         std::uint8_t hopCount, std::uint16_t routeVersion, std::uint32_t nowMs,
                         std::uint16_t advertisedMetric = 0U);
        bool updateRoute(std::uint8_t destinationId, std::uint8_t nextHopId,
                         const neighbor::LinkMetric &linkMetric, std::uint8_t hopCount,
                         std::uint16_t routeVersion, std::uint32_t nowMs);
        // Route to a field node, learned from a data packet that came from it through previousHop. An advertised route always wins.
        bool updateReverseRoute(std::uint8_t destinationId, std::uint8_t previousHopId, std::uint8_t hopCount, std::uint32_t nowMs);
        // The primary parent failed: switch to the backup parent. False when there is none (the caller then invalidates the route).
        bool promoteBackup(std::uint8_t destinationId, std::uint8_t failedNextHopId);
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
        static bool feasibleBackup_(std::uint16_t ourTotalMetric, std::uint16_t neighbourAdvertisedMetric);
        static void keepAsBackup_(RouteEntry &primary, const RouteEntry &candidate);
        static bool betterRoute_(const RouteEntry &candidate, const RouteEntry &current);
        bool isUsable_(const RouteEntry &route, std::uint32_t nowMs) const;
        std::size_t findIndex_(std::uint8_t destinationId) const;
        std::size_t selectVictim_(const RouteEntry &candidate, std::uint32_t nowMs) const;

        const std::uint8_t localNodeId_;
        const std::uint32_t routeLifetimeMs_;
        std::array<RouteEntry, constants::kRouteTableCapacity> routes_{};
    };
} // namespace terrawatch::routing
