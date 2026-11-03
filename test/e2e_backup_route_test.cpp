#include <cassert>
#include <cstdint>

#include "Constants.h"
#include "EndToEndTracker.h"
#include "Packet.h"
#include "RouteTable.h"
#include "Validator.h"

using namespace terrawatch;

static protocol::Packet dataFrom(std::uint8_t source, std::uint16_t seq) {
    protocol::Packet p; p.type = protocol::PacketType::Data; p.sourceId = source; p.destinationId = 0xFEU; p.previousHopId = source; p.sequenceNumber = seq;
    return p;
}

static void backupParentTakesOverWhenThePrimaryFails() {
    routing::RouteTable table{1U};
    // Two neighbours advertise the gateway in the same round: 0x10 is cheaper, 0x20 dearer.
    assert(table.updateRoute(0xFEU, 0x20U, 300U, 2U, 7U, 1000U));
    assert(table.updateRoute(0xFEU, 0x10U, 150U, 2U, 7U, 1100U));
    routing::RouteEntry route; assert(table.lookup(0xFEU, 1200U, route));
    assert(route.nextHopId == 0x10U && route.backupNextHopId == 0x20U && route.backupMetric == 300U);
    // A third, worse neighbour does not replace the better backup; a better backup does.
    assert(!table.updateRoute(0xFEU, 0x30U, 500U, 3U, 7U, 1300U));
    assert(table.lookup(0xFEU, 1300U, route) && route.backupNextHopId == 0x20U);
    assert(!table.updateRoute(0xFEU, 0x40U, 200U, 2U, 7U, 1400U));
    assert(table.lookup(0xFEU, 1400U, route) && route.backupNextHopId == 0x40U);
    // Refreshing the primary keeps the backup.
    assert(table.updateRoute(0xFEU, 0x10U, 150U, 2U, 7U, 1500U));
    assert(table.lookup(0xFEU, 1500U, route) && route.backupNextHopId == 0x40U);
    // Primary fails: the backup is promoted, not the route lost. A second failure has nothing left.
    assert(!table.promoteBackup(0xFEU, 0x99U));
    assert(table.promoteBackup(0xFEU, 0x10U));
    assert(table.lookup(0xFEU, 1600U, route) && route.nextHopId == 0x40U && route.backupNextHopId == 0U);
    assert(!table.promoteBackup(0xFEU, 0x40U));
    // A newer route version starts clean.
    assert(table.updateRoute(0xFEU, 0x30U, 900U, 4U, 8U, 1700U));
    assert(table.lookup(0xFEU, 1700U, route) && route.nextHopId == 0x30U && route.backupNextHopId == 0U);
}

static void reverseRoutesNeverBeatAdvertisedOnesAndAreNotAdvertised() {
    routing::RouteTable table{0x10U};
    assert(table.updateReverseRoute(5U, 5U, 1U, 1000U));
    routing::RouteEntry route; assert(table.lookup(5U, 1100U, route) && route.learned && route.nextHopId == 5U && route.hopCount == 1U);
    assert(table.updateReverseRoute(5U, 6U, 2U, 1200U) && table.lookup(5U, 1200U, route) && route.nextHopId == 6U); // latest path wins
    assert(!table.updateReverseRoute(0xFEU, 5U, 1U, 1200U) && !table.updateReverseRoute(0x10U, 5U, 1U, 1200U)); // never the gateway or ourselves
    assert(table.updateRoute(5U, 9U, 100U, 1U, 3U, 1300U)); // an advertised route replaces the learned one...
    assert(table.lookup(5U, 1300U, route) && !route.learned && route.nextHopId == 9U);
    assert(!table.updateReverseRoute(5U, 6U, 2U, 1400U)); // ...and the learned one cannot take it back
    assert(table.lookup(5U, 1400U, route) && route.nextHopId == 9U);
    assert(table.updateReverseRoute(7U, 7U, 1U, 1500U));
    assert(!table.lookup(7U, 1500U + constants::kDefaultRouteLifetimeMs, route)); // learned routes expire like any other
}

static void endToEndTrackerRetriesThenGivesUp() {
    reliable::EndToEndTracker t;
    const protocol::Packet a = dataFrom(1U, 10U);
    assert(t.track(a, 0U) && !t.track(a, 5U) && t.pendingCount() == 1U);
    protocol::Packet out;
    assert(!t.nextRetry(11999U, out));
    assert(t.nextRetry(12000U, out) && out.sequenceNumber == 10U);
    std::uint8_t retries = 99U;
    assert(t.confirm(10U, retries) && retries == 1U && t.pendingCount() == 0U && !t.confirm(10U, retries));
    assert(!t.nextRetry(100000U, out)); // confirmed: nothing to resend

    const protocol::Packet b = dataFrom(1U, 11U);
    assert(t.track(b, 0U));
    std::uint32_t now = 0U; int sent = 0;
    for (int i = 0; i < 400 && t.pendingCount() > 0U; ++i) { now += 1000U; if (t.nextRetry(now, out)) ++sent; }
    assert(sent == 3 && t.gaveUpCount() == 1U && t.pendingCount() == 0U); // 3 retries (12 s, 20 s, 30 s after each send), then it lets go
    for (std::uint16_t i = 0; i < 4U; ++i) assert(t.track(dataFrom(1U, static_cast<std::uint16_t>(100U + i)), 0U));
    assert(!t.track(dataFrom(1U, 200U), 0U)); // full: best effort, never blocks the node
}

static void receiptPacketIsValidOnlyWithTwoByteBody() {
    protocol::Packet p; p.type = protocol::PacketType::EndToEndAck; p.sourceId = 0xFEU; p.previousHopId = 0xFEU; p.destinationId = 5U; p.payloadSize = 2U;
    assert(protocol::Validator::isValid(p));
    p.payloadSize = 3U; assert(!protocol::Validator::isValid(p));
    p.payloadSize = 2U; p.destinationId = 0xFFU; assert(!protocol::Validator::isValid(p));
}

int main() {
    backupParentTakesOverWhenThePrimaryFails();
    reverseRoutesNeverBeatAdvertisedOnesAndAreNotAdvertised();
    endToEndTrackerRetriesThenGivesUp();
    receiptPacketIsValidOnlyWithTwoByteBody();
    return 0;
}
