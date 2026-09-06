#include "RouteTable.h"

#include <cmath>

namespace jalari::routing
{
    RouteTable::RouteTable(std::uint8_t localNodeId, std::uint32_t routeLifetimeMs)
        : localNodeId_(localNodeId), routeLifetimeMs_(routeLifetimeMs) {}

    bool RouteTable::updateRoute(std::uint8_t destinationId, std::uint8_t nextHopId, std::uint16_t metric,
                                 std::uint8_t hopCount, std::uint16_t routeVersion, std::uint32_t nowMs)
    {
        if (destinationId == 0U || destinationId == localNodeId_ || destinationId == 0xFFU ||
            nextHopId == 0U || nextHopId == localNodeId_ || metric == 0U || hopCount == 0U)
            return false;

        const RouteEntry candidate{destinationId, nextHopId, metric, hopCount, nowMs, routeVersion, true};
        const std::size_t existingIndex = findIndex_(destinationId);
        if (existingIndex < routes_.size())
        {
            RouteEntry &existing = routes_[existingIndex];
            if (existing.valid && existing.routeVersion != routeVersion &&
                !newerVersion_(routeVersion, existing.routeVersion)) return false;
            if (existing.valid && existing.routeVersion == routeVersion &&
                !betterRoute_(candidate, existing) &&
                (candidate.metric != existing.metric || candidate.hopCount != existing.hopCount ||
                 candidate.nextHopId != existing.nextHopId)) return false;
            existing = candidate;
            return true;
        }

        for (std::size_t i = 0; i < routes_.size(); ++i)
            if (!routes_[i].valid)
            {
                routes_[i] = candidate;
                return true;
            }

        const std::size_t victim = selectVictim_(candidate, nowMs);
        if (victim >= routes_.size()) return false;
        routes_[victim] = candidate;
        return true;
    }

    bool RouteTable::updateRoute(std::uint8_t destinationId, std::uint8_t nextHopId,
                                 const neighbor::LinkMetric &linkMetric, std::uint8_t hopCount,
                                 std::uint16_t routeVersion, std::uint32_t nowMs)
    {
        if (linkMetric.state != neighbor::MetricState::Valid ||
            !std::isfinite(linkMetric.etxCost) || linkMetric.etxCost <= 0.0F)
            return false;
        const float scaled = linkMetric.etxCost * static_cast<float>(constants::kRouteMetricScale);
        if (scaled > 65535.0F) return false;
        return updateRoute(destinationId, nextHopId, static_cast<std::uint16_t>(scaled + 0.5F),
                           hopCount, routeVersion, nowMs);
    }

    bool RouteTable::lookup(std::uint8_t destinationId, std::uint32_t nowMs, RouteEntry &outRoute) const
    {
        outRoute = {};
        const std::size_t index = findIndex_(destinationId);
        if (index >= routes_.size() || !isUsable_(routes_[index], nowMs)) return false;
        outRoute = routes_[index];
        return true;
    }

    bool RouteTable::getValidAt(std::size_t index, std::uint32_t nowMs, RouteEntry &outRoute) const
    {
        outRoute = {};
        if (index >= routes_.size() || !isUsable_(routes_[index], nowMs)) return false;
        outRoute = routes_[index];
        return true;
    }

    bool RouteTable::find(std::uint8_t destinationId, RouteEntry &outRoute) const
    {
        outRoute = {};
        const std::size_t index = findIndex_(destinationId);
        if (index >= routes_.size()) return false;
        outRoute = routes_[index];
        return true;
    }

    bool RouteTable::invalidateRoute(std::uint8_t destinationId)
    {
        const std::size_t index = findIndex_(destinationId);
        if (index >= routes_.size() || !routes_[index].valid) return false;
        routes_[index].valid = false;
        return true;
    }

    bool RouteTable::invalidateRoute(std::uint8_t destinationId, std::uint8_t expectedNextHopId)
    {
        const std::size_t index = findIndex_(destinationId);
        if (index >= routes_.size() || !routes_[index].valid ||
            routes_[index].nextHopId != expectedNextHopId)
            return false;
        routes_[index].valid = false;
        return true;
    }

    bool RouteTable::removeRoute(std::uint8_t destinationId)
    {
        const std::size_t index = findIndex_(destinationId);
        if (index >= routes_.size()) return false;
        routes_[index] = {};
        return true;
    }

    std::size_t RouteTable::expireStale(std::uint32_t nowMs)
    {
        std::size_t expired = 0;
        for (RouteEntry &route : routes_)
            if (route.valid && (nowMs - route.lastUpdatedMs) >= routeLifetimeMs_)
            {
                route.valid = false;
                ++expired;
            }
        return expired;
    }

    void RouteTable::clear() { routes_ = {}; }

    std::size_t RouteTable::count() const
    {
        std::size_t result = 0;
        for (const RouteEntry &route : routes_)
            if (route.destinationId != 0U) ++result;
        return result;
    }

    std::size_t RouteTable::validCount(std::uint32_t nowMs) const
    {
        std::size_t result = 0;
        for (const RouteEntry &route : routes_)
            if (isUsable_(route, nowMs)) ++result;
        return result;
    }

    bool RouteTable::newerVersion_(std::uint16_t candidate, std::uint16_t current)
    {
        return candidate != current && static_cast<std::uint16_t>(candidate - current) < 0x8000U;
    }

    bool RouteTable::betterRoute_(const RouteEntry &candidate, const RouteEntry &current)
    {
        if (candidate.metric != current.metric) return candidate.metric < current.metric;
        if (candidate.hopCount != current.hopCount) return candidate.hopCount < current.hopCount;
        return candidate.nextHopId < current.nextHopId;
    }

    bool RouteTable::isUsable_(const RouteEntry &route, std::uint32_t nowMs) const
    {
        return route.valid && route.destinationId != 0U && route.destinationId != localNodeId_ &&
               route.destinationId != 0xFFU && route.nextHopId != 0U && route.nextHopId != localNodeId_ &&
               route.metric != 0U && route.hopCount != 0U &&
               (nowMs - route.lastUpdatedMs) < routeLifetimeMs_;
    }

    std::size_t RouteTable::findIndex_(std::uint8_t destinationId) const
    {
        for (std::size_t i = 0; i < routes_.size(); ++i)
            if (routes_[i].destinationId == destinationId) return i;
        return routes_.size();
    }

    std::size_t RouteTable::selectVictim_(const RouteEntry &candidate, std::uint32_t nowMs) const
    {
        std::size_t victim = routes_.size();
        for (std::size_t i = 0; i < routes_.size(); ++i)
        {
            const RouteEntry &route = routes_[i];
            const bool routeStale = !isUsable_(route, nowMs);
            if (victim == routes_.size()) { victim = i; continue; }
            const RouteEntry &current = routes_[victim];
            const bool currentStale = !isUsable_(current, nowMs);
            if (routeStale != currentStale) { if (routeStale) victim = i; continue; }
            if (route.metric != current.metric) { if (route.metric > current.metric) victim = i; continue; }
            if (route.hopCount != current.hopCount) { if (route.hopCount > current.hopCount) victim = i; continue; }
            if (route.lastUpdatedMs != current.lastUpdatedMs) { if (route.lastUpdatedMs < current.lastUpdatedMs) victim = i; continue; }
            if (route.destinationId > current.destinationId) victim = i;
        }
        if (victim < routes_.size())
        {
            const RouteEntry &current = routes_[victim];
            if (newerVersion_(candidate.routeVersion, current.routeVersion)) return victim;
            if (candidate.routeVersion != current.routeVersion || !betterRoute_(candidate, current))
                return routes_.size();
        }
        return victim;
    }
} // namespace jalari::routing
