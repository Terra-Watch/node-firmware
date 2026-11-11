"""Two-tier network (the Silvanet / Lund idea): a few powered relays with their receivers always on form the tree to the gateway; the other stations are leaves that sleep except for the
acknowledgement window after each report, send to their best relay (a fixed parent chosen at install, the second best as backup) and never relay.
Relays are chosen by greedy cover so that every station has a relay within decodable range, then connected to the gateway through the cheapest path.
Compared with every station being a relay (the firmware now: dv+fix). Run: python3 two_tier.py"""
import concurrent.futures as cf, heapq, math, statistics
import mesh_sim as M
import routing_compare as R
import gateway_placement as G

GATEWAY = M.GATEWAY
I_TX, I_RX = 90.0, 10.8


class TwoTier(R.DVFixSim):
    COVER = 1        # every station must have this many relays within decodable range (the station itself counts if it is a relay)
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        n = self.n; relays = set(); count = {i: 0 for i in range(1, n + 1)}
        while any(c < self.COVER for c in count.values()):
            need = {i for i, c in count.items() if c < self.COVER}
            best = max((r for r in range(1, n + 1) if r not in relays), key=lambda r: len(({r} | set(self.nbr[r])) & need), default=None)
            if best is None or not (({best} | set(self.nbr[best])) & need): break
            relays.add(best)
            for i in {best} | set(self.nbr[best]):
                if i in count: count[i] += 1
        # connect every relay to the gateway through relays: add the nodes on the cheapest path (oracle tree) where a relay has no relay neighbour closer to the gateway
        changed = True
        while changed:
            changed = False
            cost = self.relay_cost(relays)
            for r in list(relays):
                if math.isinf(cost[r]):
                    p = M.Sim.choose_parent(self, r)[0]
                    if p is not None and p != GATEWAY and p not in relays: relays.add(p); changed = True
        self.relays = relays; cost = self.relay_cost(relays); self.leaf_parents = {}
        for leaf in range(1, n + 1):
            if leaf in relays: continue
            cands = sorted((self.etx(leaf, r) + cost[r], r) for r in self.nbr[leaf] if (r in relays or r == GATEWAY) and not math.isinf(cost[r] if r != GATEWAY else 0))
            self.leaf_parents[leaf] = [c[1] for c in cands[:2]]
        self.txtime = [0.0] * (n + 1)
    def relay_cost(self, relays):
        cost = {i: math.inf for i in range(self.n + 1)}; cost[GATEWAY] = 0.0; h = [(0.0, GATEWAY)]
        while h:
            c, u = heapq.heappop(h)
            if c > cost[u]: continue
            for v in self.nbr[u]:
                if v in relays and c + self.etx(v, u) < cost[v]: cost[v] = c + self.etx(v, u); heapq.heappush(h, (cost[v], v))
        return cost
    def transmit(self, node, frame):
        self.txtime[node] += M.AIR[frame["kind"]]; super().transmit(node, frame)
    def advert_once(self, node):
        if node in self.relays: super().advert_once(node)
    def rx_advert(self, src, dst, frame):
        if dst in self.relays: super().rx_advert(src, dst, frame)       # leaves do not listen for adverts
    def choose_parent(self, node):
        if node in self.relays or node == GATEWAY: return super().choose_parent(node)
        bad = {k for k, until in self.known_dead[node].items() if until > self.t}
        ps = [p for p in self.leaf_parents.get(node, []) if p not in bad]
        # a relay that has lost its own route is no use either
        ps = [p for p in ps if p == GATEWAY or self.valid(p)] or ps
        return (ps[0] if ps else None), (ps[1] if len(ps) > 1 else None)


class TwoTier2(TwoTier):
    COVER = 2


def one(args):
    algo, layout, n, seed, fail = args
    M.Sim.place = G.place_with(G.LAYOUTS[layout])
    cls, cfg = ({"two-tier": TwoTier, "two-tier-2": TwoTier2}[algo], M.CONFIGS["full"]) if algo.startswith("two-tier") else R.ALGOS[algo]
    try: sim = cls(n, 600, cfg, seed, 3600.0, fail)
    except RuntimeError: return (algo, layout, n, fail), None
    r = sim.run()
    if algo.startswith("two-tier"): r["n_relays"] = len(sim.relays)
    return (algo, layout, n, fail), r


def main():
    jobs = [(a, l, n, s, f) for a in ("dv+fix", "two-tier", "two-tier-2") for l in ("corner", "centre") for n in (40, 80) for s in range(1, 11) for f in (False, True)]
    groups = {}
    with cf.ProcessPoolExecutor() as pool:
        for k, r in pool.map(one, jobs, chunksize=2):
            if r: groups.setdefault(k, []).append(r)
    m = lambda rs, key: statistics.mean(r[key] for r in rs if r.get(key) is not None)
    rx_day = I_RX * 24.0
    print(f"{'gateway':<7} {'stations':>8} {'design':<9} {'relays':>6} {'delivered':>10} {'relay dies':>11} {'within 60s':>11} {'tx/packet':>9} {'busy':>6} {'radio mAh/day, whole network (RX always-on relays only)':>14}")
    for l in ("corner", "centre"):
        for n in (40, 80):
            for a in ("dv+fix", "two-tier", "two-tier-2"):
                ok, bad = groups.get((a, l, n, False), []), groups.get((a, l, n, True), [])
                if not ok or not bad: continue
                k = m(ok, "n_relays") if a.startswith("two-tier") else n
                energy = k * rx_day + (n - k) * 0.5 + n * 2.0 if a.startswith("two-tier") else n * rx_day + n * 2.5
                print(f"{l:<7} {n:>8} {a:<9} {k:>6.0f} {m(ok, 'pdr') * 100:9.1f}% {m(bad, 'pdr') * 100:10.1f}% {m(bad, 'fail_60s') * 100:10.1f}% {m(ok, 'tx_per_delivered'):9.1f} {m(ok, 'air_busy') * 100:5.1f}% {energy:14.0f}")


if __name__ == "__main__":
    main()
