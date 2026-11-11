// Validation of the real routing code (RouteTable, RouteAdvertisementService, NeighborManager, PacketFactory) on random networks, against Dijkstra.
// Each node runs the firmware classes. Link costs are what the firmware computes: ETX = 1 / (ack successes of the last 16). Adverts are lost with the link's
// failure rate and arrive after a random delay; gateway adverts go out at 3, 10, 25 s and then every 60 s; every node adverts at its own phase once per 60 s.
// Measured after convergence: routes that exist, loops, dead ends, path cost against the best possible (Dijkstra) and, when a relay dies, how many nodes have a backup parent that works.
// Run with test/run_route_convergence_tests.sh (sanitizers on, fixed seeds).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <vector>

#include "Config.h"
#include "NeighborManager.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "RouteAdvertisementService.h"

using namespace terrawatch;
namespace tc = terrawatch::constants;

struct Clock final : hal::IClock { std::uint32_t millis() const override { return now; } std::uint32_t now = 0; };
struct Node {
    std::uint8_t id; config::NodeConfig cfg; node::NodeManager manager; protocol::PacketFactory factory; neighbor::NeighborManager neighbors; routing::RouteTable routes; queue::PacketQueue queue; Clock clock; routing::RouteAdvertisementService service;
    explicit Node(std::uint8_t i, const config::NetworkConfig &net) : id(i), cfg{i}, manager(cfg, node::DeviceRole::FieldNode), factory(manager, net), routes(i, 100000000U), service(i, routes, neighbors, factory, queue, clock, 1000000000U, 60000U) {}
};
struct Ev { std::uint32_t t; int kind; int a; int b; protocol::Packet pkt; bool operator>(const Ev &o) const { return t > o.t; } };   // kind 0: node tick, 1: gateway advert, 2: delivery

struct Result { int nodes = 0, reached = 0, loops = 0, deadends = 0, optimal = 0, backups = 0, backups_ok = 0, orphans = 0; std::vector<double> stretch; int backup_in_dead_subtree = 0; std::vector<double> working; /* share of live, still connected nodes with a working path, at 10, 70, 130 and 310 s after the relay died */ };

static Result run(int n, double radius_km, double area_km, std::uint64_t seed, bool kill_relay) {
    std::mt19937_64 rng(seed); std::uniform_real_distribution<double> U(0, 1);
    const config::NetworkConfig net{}; std::vector<std::unique_ptr<Node>> nodes; nodes.push_back(nullptr);
    for (int i = 1; i <= n; ++i) nodes.push_back(std::make_unique<Node>(static_cast<std::uint8_t>(i), net));
    config::NodeConfig gcfg{0xFEU}; node::NodeManager gman(gcfg, node::DeviceRole::Gateway); protocol::PacketFactory gfactory(gman, net);
    std::vector<std::pair<double, double>> pos(n + 1); pos[0] = {0, 0};
    for (int i = 1; i <= n; ++i) pos[i] = {U(rng) * area_km, U(rng) * area_km};           // node 0 is the gateway in the corner
    // symmetric success counts k of 16 (0 = no link); a link exists when the receive probability is at least 2/16
    std::vector<std::vector<int>> k(n + 1, std::vector<int>(n + 1, 0));
    for (int a = 0; a <= n; ++a) for (int b = a + 1; b <= n; ++b) {
        double d = std::hypot(pos[a].first - pos[b].first, pos[a].second - pos[b].second); double p = 1.0 / (1.0 + std::pow(d / radius_km, 6.0)) ;       // falls from 1 to 0 around the radius
        p = std::clamp(p * (0.9 + 0.2 * U(rng)), 0.0, 1.0); int kk = static_cast<int>(std::lround(p * 16)); if (kk >= 2) k[a][b] = k[b][a] = kk;
    }
    // neighbour tables: at most 16 neighbours (the firmware limit), the best ones
    std::vector<std::vector<int>> nb(n + 1);
    for (int a = 1; a <= n; ++a) { std::vector<int> c; for (int b = 0; b <= n; ++b) if (b != a && k[a][b]) c.push_back(b); std::sort(c.begin(), c.end(), [&](int x, int y) { return k[a][x] > k[a][y]; }); if (c.size() > tc::kNeighborCapacity) c.resize(tc::kNeighborCapacity); nb[a] = c; }
    auto gid = [](int i) { return i == 0 ? tc::kGatewayNodeId : static_cast<std::uint8_t>(i); };
    for (int a = 1; a <= n; ++a) for (int b : nb[a]) if (b != 0) {     // the gateway sends no heartbeats: it is never a measured neighbour
        protocol::Packet hb; hb.type = protocol::PacketType::Heartbeat; hb.sourceId = gid(b); hb.sequenceNumber = 1; nodes[a]->neighbors.observeHeartbeat(hb, -90, 5.0F, 0U, 8U);
        for (int s = 0; s < 16; ++s) { if (s < k[a][b]) nodes[a]->neighbors.recordAcknowledgementSuccess(gid(b), 1U); else nodes[a]->neighbors.recordAcknowledgementFailure(gid(b), 1U); } }
    // true costs for the ground truth: ETX = 16 / k, symmetric; the gateway's link uses its real value
    auto cost = [&](int a, int b) { return 16.0 / k[a][b]; };
    std::vector<double> best(n + 1, 1e18); best[0] = 0; std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pq; pq.push({0, 0});
    while (!pq.empty()) { auto [c, u] = pq.top(); pq.pop(); if (c > best[u]) continue; for (int v = 0; v <= n; ++v) if (v != u && k[u][v] && c + cost(u, v) < best[v]) { best[v] = c + cost(u, v); pq.push({best[v], v}); } }
    std::priority_queue<Ev, std::vector<Ev>, std::greater<>> q;
    for (std::uint32_t t : {3000U, 10000U, 25000U}) q.push({t, 1, 0, 0, {}});
    for (std::uint32_t t = 60000; t <= 600000; t += 60000) q.push({t, 1, 0, 0, {}});
    std::vector<std::uint32_t> phase(n + 1); for (int i = 1; i <= n; ++i) { phase[i] = static_cast<std::uint32_t>(U(rng) * 60000); for (std::uint32_t t = phase[i]; t <= 610000; t += 60000) q.push({t, 0, i, 0, {}}); }
    std::uint16_t version = 0; std::vector<char> dead(n + 1, 0); const std::uint32_t kill_at = 400000; bool killed = false, promoted = false; int victim = -1; std::vector<char> cut(n + 1, 0);
    auto deliver = [&](int from, int to, protocol::Packet pkt, std::uint32_t now) { if (U(rng) * 16 >= k[from][to]) return; pkt.previousHopId = gid(from); q.push({now + 5 + static_cast<std::uint32_t>(U(rng) * 400), 2, from, to, pkt}); };
    auto pick_victim = [&]() { std::vector<int> kids(n + 1, 0); for (int a = 1; a <= n; ++a) { routing::RouteEntry r; if (nodes[a]->routes.find(tc::kGatewayNodeId, r) && r.valid && r.nextHopId != tc::kGatewayNodeId) ++kids[r.nextHopId]; } return static_cast<int>(std::max_element(kids.begin(), kids.end()) - kids.begin()); };
    // follow the next hops of node a: 1 = reaches the gateway over live nodes, 0 = loop or dead end; cost in c
    auto follow = [&](int a, std::uint32_t now, double &c, int &kind) { c = 0; int cur = a, steps = 0; while (steps++ <= n + 1) { routing::RouteEntry rr; if (!nodes[cur]->routes.lookup(tc::kGatewayNodeId, now, rr)) { kind = 2; return false; } int nh = rr.nextHopId == tc::kGatewayNodeId ? 0 : rr.nextHopId; if (nh < 0 || nh > n || (nh && dead[nh]) || !k[cur][nh]) { kind = 2; return false; } c += cost(cur, nh); if (nh == 0) { kind = 0; return true; } cur = nh; } kind = 1; return false; };
    std::vector<char> reachable(n + 1, 0);    // still connected to the gateway once the victim is gone
    auto compute_reachable = [&]() { std::fill(reachable.begin(), reachable.end(), 0); std::queue<int> bq; bq.push(0); reachable[0] = 1; while (!bq.empty()) { int u = bq.front(); bq.pop(); for (int v = 1; v <= n; ++v) if (!reachable[v] && !dead[v] && k[u][v]) { reachable[v] = 1; bq.push(v); } } };
    const std::uint32_t cover_at[] = {70000, 130000, 310000, 590000}; int next_cover = 0; Result res; const std::uint32_t sample_at[] = {kill_at + 10000, kill_at + 70000, kill_at + 130000, kill_at + 310000}; int next_sample = 0; const std::uint32_t horizon = kill_relay ? 720000U : 600000U;
    if (kill_relay) { for (std::uint32_t t = 660000; t <= horizon; t += 60000) q.push({t, 1, 0, 0, {}}); for (int i = 1; i <= n; ++i) for (std::uint32_t t = 610000 + phase[i]; t <= horizon + 60000; t += 60000) q.push({t, 0, i, 0, {}}); }
    while (!q.empty()) {
        Ev e = q.top(); q.pop(); if (e.t > (kill_relay ? horizon + 60000 : 600000U)) break;
        if (kill_relay && !killed && e.t >= kill_at) { killed = true; victim = pick_victim(); dead[victim] = 1; compute_reachable(); }
        while (kill_relay && killed && next_sample < 4 && e.t >= sample_at[next_sample]) {
            std::uint32_t now = sample_at[next_sample];
            if (!promoted) { promoted = true; std::vector<char> orphan(n + 1, 0); for (int a = 1; a <= n; ++a) { routing::RouteEntry r0; if (!dead[a] && nodes[a]->routes.lookup(tc::kGatewayNodeId, now, r0) && r0.nextHopId == gid(victim)) orphan[a] = 1; } for (int a = 1; a <= n; ++a) { routing::RouteEntry r; if (dead[a]) continue; if (nodes[a]->routes.lookup(tc::kGatewayNodeId, now, r) && r.nextHopId == gid(victim)) { ++res.orphans; if (r.backupNextHopId != 0 && r.backupNextHopId != tc::kGatewayNodeId && r.backupNextHopId <= n && orphan[r.backupNextHopId]) ++res.backup_in_dead_subtree; if (nodes[a]->routes.promoteBackup(tc::kGatewayNodeId, gid(victim))) ++res.backups; else nodes[a]->routes.invalidateRoute(tc::kGatewayNodeId); } } }
            int good = 0, total = 0; for (int a = 1; a <= n; ++a) { if (dead[a] || !reachable[a]) continue; ++total; double c; int kind; if (follow(a, now, c, kind)) ++good; }
            res.working.push_back(total ? static_cast<double>(good) / total : 1.0); ++next_sample;
        }
        while (!kill_relay && next_cover < 4 && e.t >= cover_at[next_cover]) { std::uint32_t now = cover_at[next_cover]; int good = 0, total = 0; for (int a = 1; a <= n; ++a) { if (best[a] > 1e17) continue; ++total; double c; int kind; if (follow(a, now, c, kind)) ++good; } res.working.push_back(total ? static_cast<double>(good) / total : 1.0); ++next_cover; }
        if (e.kind == 1) { ++version; protocol::Packet p; gfactory.createRouteAdvertisement(tc::kGatewayNodeId, 100U, 1U, version, p); for (int b = 1; b <= n; ++b) if (k[0][b] && !dead[b]) deliver(0, b, p, e.t); }
        else if (e.kind == 0) { int a = e.a; if (dead[a]) continue; nodes[a]->clock.now = e.t; nodes[a]->service.update(); protocol::Packet p; while (nodes[a]->queue.dequeue(p)) for (int b : nb[a]) if (b != 0 && !dead[b]) deliver(a, b, p, e.t); }
        else { int b = e.b; if (dead[b]) continue; nodes[b]->clock.now = e.t; nodes[b]->service.processReceived(e.pkt, e.t); }
    }
    res.nodes = n;
    if (!kill_relay) for (int a = 1; a <= n; ++a) { if (best[a] > 1e17) continue; double c; int kind; if (follow(a, 600000, c, kind)) { ++res.reached; res.stretch.push_back(c / best[a]); if (c <= best[a] * 1.0001) ++res.optimal; } else if (kind == 1) ++res.loops; else ++res.deadends; }
    return res;
}

int main(int argc, char **argv) {
    int failures = 0; std::printf("Steady state at 10 minutes (30 random networks per row). deadends = no route yet, or a next hop that no longer works; stretch = path cost over the best possible (Dijkstra):\n%-34s %6s %8s %7s %9s %8s %8s %8s   %s\n", "case", "nodes", "reached", "loops", "deadends", "optimal", "mean", "p95", "with a route at 70/130/310/590 s");
    struct Case { int n; double radius, area; } cases[] = {{10, 1.2, 3.0}, {20, 1.2, 4.0}, {40, 1.2, 6.0}, {60, 1.2, 7.0}, {40, 0.8, 5.0}};
    for (auto &c : cases) {
        Result tot; int runs = 30; double cov[4] = {0, 0, 0, 0}; int cn = 0;
        for (int s = 0; s < runs; ++s) { Result r = run(c.n, c.radius, c.area, 100 + s, false); tot.nodes += r.nodes; tot.reached += r.reached; tot.loops += r.loops; tot.deadends += r.deadends; tot.optimal += r.optimal; tot.stretch.insert(tot.stretch.end(), r.stretch.begin(), r.stretch.end()); if (r.working.size() == 4) { for (int m = 0; m < 4; ++m) cov[m] += r.working[m]; ++cn; } }
        std::sort(tot.stretch.begin(), tot.stretch.end()); double mean = 0; for (double x : tot.stretch) mean += x; mean /= std::max<size_t>(1, tot.stretch.size());
        double p95 = tot.stretch.empty() ? 0 : tot.stretch[static_cast<size_t>(0.95 * (tot.stretch.size() - 1))];
        char label[64]; std::snprintf(label, sizeof label, "%d nodes, radius %.1f km, %.0fx%.0f km", c.n, c.radius, c.area, c.area);
        std::printf("%-34s %6d %7.1f%% %7d %9d %7.1f%% %8.3f %8.3f   %.0f%% / %.0f%% / %.0f%% / %.0f%%\n", label, tot.nodes, 100.0 * tot.reached / std::max(1, tot.reached + tot.loops + tot.deadends), tot.loops, tot.deadends, 100.0 * tot.optimal / std::max(1, tot.reached), mean, p95, 100 * cov[0] / std::max(1, cn), 100 * cov[1] / std::max(1, cn), 100 * cov[2] / std::max(1, cn), 100 * cov[3] / std::max(1, cn));
        if (tot.loops > 0) { ++failures; std::printf("FAIL: %d routing loops\n", tot.loops); }
    }
    std::printf("\nRelay failure: the relay with most children dies at 400 s, its children switch to their backup parent 10 s later. Share of the nodes that are still connected and have a working path:\n%-34s %8s %10s %12s %9s %9s %9s %9s\n", "case", "orphans", "had backup", "backup also", "+10 s", "+70 s", "+130 s", "+310 s");
    for (auto &c : cases) {
        int orphans = 0, had = 0, bad = 0; double w[4] = {0, 0, 0, 0}; int runs = 30, counted = 0;
        for (int s = 0; s < runs; ++s) { Result r = run(c.n, c.radius, c.area, 100 + s, true); orphans += r.orphans; had += r.backups; bad += r.backup_in_dead_subtree; if (r.working.size() == 4) { for (int m = 0; m < 4; ++m) w[m] += r.working[m]; ++counted; } }
        char label[64]; std::snprintf(label, sizeof label, "%d nodes, radius %.1f km, %.0fx%.0f km", c.n, c.radius, c.area, c.area);
        std::printf("%-34s %8d %9.1f%% %11.1f%% %8.1f%% %8.1f%% %8.1f%% %8.1f%%\n", label, orphans, orphans ? 100.0 * had / orphans : 0.0, orphans ? 100.0 * bad / orphans : 0.0, 100 * w[0] / std::max(1, counted), 100 * w[1] / std::max(1, counted), 100 * w[2] / std::max(1, counted), 100 * w[3] / std::max(1, counted));
    }
    (void)argc; (void)argv; return failures ? 1 : 0;
}
