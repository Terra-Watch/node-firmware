#include <cassert>
#include <cmath>

#include "NeighborManager.h"
#include "RouteTable.h"

using namespace jalari;

static neighbor::LinkMetric validMetric(float cost) {
    neighbor::LinkMetric metric;
    metric.state = neighbor::MetricState::Valid;
    metric.etxCost = cost;
    metric.acknowledgementSuccessProbability = 1.0F / cost;
    metric.observationCount = 2U;
    return metric;
}

int main() {
    routing::RouteTable table(0x01U, 100U);
    routing::RouteEntry route;
    assert(!table.lookup(0xFEU, 0U, route));

    assert(table.updateRoute(0xFEU, 0x02U, 500U, 3U, 1U, 10U));
    assert(table.lookup(0xFEU, 10U, route));
    assert(route.nextHopId == 0x02U && route.metric == 500U && route.hopCount == 3U);

    assert(table.updateRoute(0x03U, 0x04U, 200U, 2U, 1U, 10U));
    assert(table.updateRoute(0x05U, 0x06U, 300U, 1U, 1U, 10U));
    assert(table.lookup(0x03U, 10U, route) && route.nextHopId == 0x04U);
    assert(table.lookup(0x05U, 10U, route) && route.nextHopId == 0x06U);

    // Same-version quality updates are deterministic; worse candidates are rejected.
    assert(table.updateRoute(0xFEU, 0x03U, 200U, 3U, 1U, 20U));
    assert(table.lookup(0xFEU, 20U, route) && route.nextHopId == 0x03U);
    assert(!table.updateRoute(0xFEU, 0x04U, 500U, 3U, 1U, 30U));
    assert(table.updateRoute(0xFEU, 0x02U, 900U, 3U, 2U, 30U));
    assert(table.lookup(0xFEU, 30U, route) && route.routeVersion == 2U);
    assert(table.updateRoute(0xFEU, 0x02U, 900U, 3U, 2U, 40U)); // refresh
    assert(table.lookup(0xFEU, 130U, route));
    assert(!table.lookup(0xFEU, 140U, route));
    assert(table.expireStale(140U) == 3U);
    assert(table.find(0xFEU, route) && !route.valid);
    assert(!table.invalidateRoute(0x03U));
    assert(!table.lookup(0x03U, 40U, route));

    // Equal-cost ties use hop count and then next-hop ID.
    routing::RouteTable ties(0x01U, 1000U);
    assert(ties.updateRoute(0x10U, 0x09U, 200U, 3U, 1U, 0U));
    assert(ties.updateRoute(0x10U, 0x08U, 200U, 3U, 1U, 1U));
    assert(ties.lookup(0x10U, 1U, route) && route.nextHopId == 0x08U);

    // Fixed capacity: a better candidate replaces the deterministic worst entry.
    routing::RouteTable full(0x01U, 1000U);
    for (std::uint8_t destination = 2U; destination < 18U; ++destination)
        assert(full.updateRoute(destination, 0x20U, static_cast<std::uint16_t>(100U + destination), 1U, 1U, 0U));
    assert(full.count() == constants::kRouteTableCapacity);
    assert(full.updateRoute(0x30U, 0x20U, 50U, 1U, 1U, 0U));
    assert(full.count() == constants::kRouteTableCapacity && full.lookup(0x30U, 0U, route));

    routing::RouteTable stale(0x01U, 100U);
    for (std::uint8_t destination = 2U; destination < 18U; ++destination)
        assert(stale.updateRoute(destination, 0x20U, 100U, 1U, 1U, 0U));
    assert(stale.updateRoute(0x31U, 0x20U, 100U, 1U, 2U, 101U));
    assert(stale.count() == constants::kRouteTableCapacity && stale.lookup(0x31U, 101U, route));

    // Self, broadcast, zero, and invalid metrics never create usable routes.
    routing::RouteTable guards(0x01U, 100U);
    assert(!guards.updateRoute(0x01U, 0x02U, 100U, 1U, 1U, 0U));
    assert(!guards.updateRoute(0xFFU, 0x02U, 100U, 1U, 1U, 0U));
    assert(!guards.updateRoute(0U, 0x02U, 100U, 1U, 1U, 0U));
    assert(!guards.updateRoute(0xFEU, 0x01U, 100U, 1U, 1U, 0U));
    neighbor::LinkMetric unknown;
    assert(!guards.updateRoute(0xFEU, 0x02U, unknown, 1U, 1U, 0U));
    assert(guards.count() == 0U);

    const auto metric = validMetric(1.25F);
    assert(guards.updateRoute(0xFEU, 0x02U, metric, 1U, 1U, 0U));
    assert(guards.lookup(0xFEU, 0U, route));
    assert(route.metric == 125U);
    assert(std::fabs(static_cast<float>(route.metric) / constants::kRouteMetricScale - 1.25F) < 0.001F);
    assert(guards.updateRoute(0x10U, 0x02U, metric, 1U, 1U, 0U));
    assert(guards.count() == 2U);
    return 0;
}
