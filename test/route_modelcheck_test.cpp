// Exhaustive model check of the real routing code (RouteTable) on small networks: every order of advert sends and deliveries, every gateway version, is explored (explicit-state breadth-first search,
// states hashed). Checks, in every reachable state:
//   P1  no cycle in the next-hop graph of the primary routes (loop freedom)
//   P2  following the backup parent of any node and then the primary routes does not lead back to that node (a failover cannot create a loop)
//   P3  every node's route metric is at least its next hop's metric (costs fall towards the gateway)
// and counts the states in which P2 fails. Delivery uses the firmware's rule (RouteAdvertisementService::processReceived): link cost x100 + advertised metric, hop+1, one advert per (source, version) per receiver.
// Messages can be delayed and reordered (they sit in flight until delivered, with the content they had when sent); losing one is the same as never delivering it.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "RouteTable.h"

using namespace terrawatch;
namespace tc = terrawatch::constants;

struct Msg { int src, dst; std::uint16_t metric; std::uint8_t hops; std::uint16_t ver; bool operator<(const Msg &o) const { return std::tie(src, dst, metric, hops, ver) < std::tie(o.src, o.dst, o.metric, o.hops, o.ver); } };
struct World {
    int n;                                           // nodes 1..n; the gateway is index 0 here (0xFE on the wire)
    std::vector<routing::RouteTable> rt;
    std::vector<std::set<std::pair<int, int>>> seen; // per receiver: (source, version) already used
    std::set<Msg> inflight; std::set<std::pair<int, int>> sent;   // sent: (source, version) already advertised once from a given content
    int version = 0;
};
static std::uint8_t wire(int i) { return i == 0 ? tc::kGatewayNodeId : static_cast<std::uint8_t>(i); }
static int unwire(std::uint8_t w) { return w == tc::kGatewayNodeId ? 0 : w; }

struct Topo { int n; std::vector<std::vector<int>> cost; };      // cost[a][b] = link ETX (1 or 2) or 0 for no link

static std::string key(const World &w) {
    std::string s;
    for (int i = 1; i <= w.n; ++i) { routing::RouteEntry r; bool f = w.rt[i].find(tc::kGatewayNodeId, r); s += f && r.valid ? std::to_string(r.nextHopId) + ',' + std::to_string(r.metric) + ',' + std::to_string(r.hopCount) + ',' + std::to_string(r.routeVersion) + ',' + std::to_string(r.backupNextHopId) + ',' + std::to_string(r.backupMetric) : "-"; s += ';'; }
    for (int i = 1; i <= w.n; ++i) { for (auto &p : w.seen[i]) s += std::to_string(p.first) + ':' + std::to_string(p.second) + ' '; s += '|'; }
    for (auto &m : w.inflight) s += std::to_string(m.src) + '>' + std::to_string(m.dst) + ':' + std::to_string(m.metric) + ',' + std::to_string(m.hops) + ',' + std::to_string(m.ver) + ' ';
    s += '#'; for (auto &p : w.sent) s += std::to_string(p.first) + ':' + std::to_string(p.second) + ' ';
    s += '@' + std::to_string(w.version); return s;
}

static bool route_of(const World &w, int i, routing::RouteEntry &r) { return i != 0 && w.rt[i].find(tc::kGatewayNodeId, r) && r.valid; }

int main(int argc, char **argv) {
    int violations_p1 = 0, violations_p2 = 0, violations_p3 = 0; long total_states = 0, p2_states = 0;
    struct Case { const char *name; Topo t; int versions; } cases[] = {
        {"diamond (G-1, G-2, 1-3, 2-3)", {3, {{0, 1, 1, 0}, {1, 0, 0, 1}, {1, 0, 0, 1}, {0, 1, 1, 0}}}, 2},
        {"triangle plus tail, unequal costs", {3, {{0, 1, 2, 0}, {1, 0, 1, 2}, {2, 1, 0, 1}, {0, 2, 1, 0}}}, 2},
        {"diamond with a tail (G-1, G-2, 1-3, 2-3, 3-4)", {4, {{0, 1, 1, 0, 0}, {1, 0, 0, 1, 0}, {1, 0, 0, 1, 0}, {0, 1, 1, 0, 1}, {0, 0, 0, 1, 0}}}, 1},
    };
    (void)argc; (void)argv;
    for (auto &c : cases) {
        const Topo &t = c.t; int n = t.n;
        World w0; w0.n = n; for (int i = 0; i <= n; ++i) w0.rt.emplace_back(wire(i), 1000000000U); w0.seen.resize(n + 1);
        std::deque<World> q; std::unordered_set<std::string> visited; q.push_back(w0); visited.insert(key(w0));
        long states = 0, p2 = 0, p1 = 0, p3 = 0; bool printed = false;
        while (!q.empty() && states < 1500000) {
            World w = q.front(); q.pop_front(); ++states;
            // properties
            for (int a = 1; a <= n; ++a) {
                routing::RouteEntry r; if (!route_of(w, a, r)) continue;
                int cur = a, steps = 0; bool loop = false; while (steps++ <= n + 1) { routing::RouteEntry rr; if (cur == 0) break; if (!route_of(w, cur, rr)) break; cur = unwire(rr.nextHopId); if (cur == a) { loop = true; break; } }
                if (loop) ++p1;
                if (r.nextHopId != tc::kGatewayNodeId) { routing::RouteEntry nr; if (route_of(w, unwire(r.nextHopId), nr) && r.metric < nr.metric) ++p3; }
                if (r.backupNextHopId != 0) {
                    int b = unwire(r.backupNextHopId); int cur2 = b, st = 0; bool back = false; while (st++ <= n + 1 && cur2 != 0) { if (cur2 == a) { back = true; break; } routing::RouteEntry rr; if (!route_of(w, cur2, rr)) break; cur2 = unwire(rr.nextHopId); }
                    if (back) { ++p2; if (!printed) { printed = true; std::printf("  first backup loop: node %d has backup %d whose own route leads back to %d\n", a, b, a); } }
                }
            }
            // transitions
            auto push = [&](World &nw) { std::string k = key(nw); if (visited.insert(k).second) q.push_back(nw); };
            if (w.version < c.versions) {          // the gateway starts a new version
                World nw = w; ++nw.version;
                for (int b = 1; b <= n; ++b) if (t.cost[0][b]) nw.inflight.insert({0, b, 100, 1, static_cast<std::uint16_t>(nw.version)});
                push(nw);
            }
            for (int a = 1; a <= n; ++a) {          // a node with a route sends its advert (content as of now)
                routing::RouteEntry r; if (!route_of(w, a, r)) continue;
                World nw = w; bool added = false;
                for (int b = 1; b <= n; ++b) if (b != a && t.cost[a][b]) added |= nw.inflight.insert({a, b, r.metric, r.hopCount, r.routeVersion}).second;
                if (added) push(nw);
            }
            for (auto it = w.inflight.begin(); it != w.inflight.end(); ++it) {   // deliver one message
                World nw = w; Msg m = *it; nw.inflight.erase(m);
                if (nw.seen[m.dst].count({m.src, m.ver})) { push(nw); continue; }      // duplicate of (source, version): suppressed
                nw.seen[m.dst].insert({m.src, m.ver});
                int link = t.cost[m.src][m.dst]; double total = link * static_cast<double>(tc::kRouteMetricScale) + m.metric;
                if (m.src != 0 || true) nw.rt[m.dst].updateRoute(tc::kGatewayNodeId, wire(m.src), static_cast<std::uint16_t>(total + 0.5), static_cast<std::uint8_t>(m.hops + 1), m.ver, 0U, static_cast<std::uint16_t>(m.metric));
                push(nw);
            }
        }
        std::printf("%-46s versions %d: %8ld states%s | loops (P1) %ld, failover loops (P2) %ld, metric order (P3) %ld\n", c.name, c.versions, states, q.empty() ? "" : " (cut off)", p1, p2, p3);
        total_states += states; violations_p1 += static_cast<int>(p1); violations_p2 += static_cast<int>(p2 > 0); violations_p3 += static_cast<int>(p3); p2_states += p2;
    }
    std::printf("total %ld states; P1 violations %d, P3 violations %d; cases where a backup can create a loop: %d\n", total_states, violations_p1, violations_p3, violations_p2);
    return (violations_p1 || violations_p3) ? 1 : 0;
}
