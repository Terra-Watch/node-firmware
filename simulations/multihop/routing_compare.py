"""Why this routing algorithm: five ways to get station packets to one gateway, on the same radio model and the same random forests.

  flood     managed flooding: every node rebroadcasts each new packet once (no routes, no acknowledgements)
  static    a collection tree computed once at install and never updated (what a hand-configured network is)
  oracle    the collection tree with a perfect, always current view of all link costs (what mesh_sim.py "full" assumed; unreachable in practice)
  dv        what the firmware does: a collection tree built from route adverts (version numbers, ETX, one advert per version per neighbour, a backup parent, 180 s route lifetime),
            listen-before-talk, hop acknowledgements, end-to-end receipt for alerts
  dv+fix    dv with the two changes found in docs/24: a backup parent that does not route through our own parent, and adverts at 3, 10 and 25 s after boot
  dv-300, dv+fix-300   the same two with adverts every 300 s instead of 60 s (route lifetime 900 s): far less advert airtime

The radio, the layouts, the traffic and the failure are those of mesh_sim.py (checked against ALOHA theory and the Semtech airtime formula: validate_sim.py).
Run: python3 routing_compare.py            (about 6 minutes)     python3 routing_compare.py quick"""
import concurrent.futures as cf, itertools, json, math, statistics, sys
import mesh_sim as M

GATEWAY = M.GATEWAY
ROUTE_LIFETIME = 180.0
FLOOD_TTL = 12


class Counting(M.Sim):
    """Adds the number of transmissions and cold-start delivery to the report."""
    def __init__(self, *a, **k):
        super().__init__(*a, **k); self.ntx = 0
    def transmit(self, node, frame):
        self.ntx += 1; super().transmit(node, frame)
    def report(self):
        out = super().report()
        delivered = len(self.delivered); out["tx_per_delivered"] = self.ntx / max(delivered, 1)
        cold = [k for k, v in self.gen.items() if v < 300.0 and k not in self.alerts]
        out["pdr_cold"] = sum(1 for k in cold if k in self.delivered) / len(cold) if cold else None
        late = [k for k, v in self.gen.items() if v >= 300.0 and k not in self.alerts and v <= self.T]
        out["pdr_warm"] = sum(1 for k in late if k in self.delivered) / len(late) if late else None
        timely = lambda ks: (sum(1 for k in ks if k in self.delivered and self.delivered[k] - self.gen[k] <= 60.0) / len(ks)) if ks else None
        out["cold_60s"] = timely(cold)
        if self.fail:
            after = [k for k, v in self.gen.items() if self.T / 2 <= v < self.T / 2 + 300.0 and k not in self.alerts and k[0] not in self.dead and k[0] not in self.cut_off]
            out["fail_60s"] = timely(after)
        else: out["fail_60s"] = None
        return out


class FloodSim(Counting):
    def on_advert(self, node): return
    def send_data(self, node, frame):
        f = dict(frame); f["to"] = None; f["bcast"] = True; self.flooded = getattr(self, "flooded", set()); self.flooded.add((node, frame["origin"], frame["pid"]))
        self.enqueue(node, f)
    def on_txend(self, node, frame, start):
        if not frame.get("bcast"): return super().on_txend(node, frame, start)
        for b in self.nbr[node]:
            if b in self.dead or not self.received(node, b, start, self.t): continue
            key = (frame["origin"], frame["pid"])
            if b == GATEWAY:
                self.delivered.setdefault(key, self.t); continue
            if (b,) + key in self.flooded: continue
            self.flooded.add((b,) + key)
            if frame["hops"] + 1 > FLOOD_TTL: continue
            f = dict(frame); f["hops"] = frame["hops"] + 1; self.enqueue(b, f)


class StaticSim(Counting):
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.fixed = [None] * (self.n + 1)
        for node in range(1, self.n + 1): self.fixed[node] = M.Sim.choose_parent(self, node)[0]
    def choose_parent(self, node): return self.fixed[node], None


class OracleSim(Counting):
    pass


class DVSim(Counting):
    """Routing as the firmware does it (RouteTable.cpp, RouteAdvertisementService.cpp)."""
    indep_backup = False
    fast_boot = False
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.rt = [None] * (self.n + 1); self.version = 0; self.seen = [dict() for _ in range(self.n + 1)]
        self.lifetime = max(ROUTE_LIFETIME, 3 * self.cfg["advert"])
    def run(self):
        for t in (3.0, 10.0, 25.0): self.at(t, "gwadvert", False)
        self.at(25.0 + self.cfg["advert"], "gwadvert", True)
        if self.fast_boot:
            for node in range(1, self.n + 1):
                for t in (3.0, 10.0, 25.0): self.at(t + self.rng.uniform(0, 2.0), "advert_once", node)
        return super().run()
    def valid(self, node):
        r = self.rt[node]
        return r is not None and self.t - r["t"] <= self.lifetime
    def on_gwadvert(self, repeat):
        self.version += 1
        self.enqueue(GATEWAY, dict(kind="advert", origin=GATEWAY, cost=0.0, ver=self.version, via=None), droppable=True)
        if repeat: self.at(self.t + self.cfg["advert"], "gwadvert", True)
    def on_advert(self, node):
        if node in self.dead or self.t > self.T: return
        self.advert_once(node)
        self.at(self.t + self.cfg["advert"], "advert", node)
    def on_advert_once(self, node): self.advert_once(node)
    def advert_once(self, node):
        if node in self.dead or not self.valid(node): return
        r = self.rt[node]
        self.enqueue(node, dict(kind="advert", origin=node, cost=r["cost"], ver=r["ver"], via=r["parent"]), droppable=True)
    def on_txend(self, node, frame, start):
        if frame["kind"] != "advert": return super().on_txend(node, frame, start)
        for b in self.nbr[node]:
            if b == GATEWAY or b in self.dead: continue
            if self.received(node, b, start, self.t): self.rx_advert(node, b, frame)
    def rx_advert(self, src, dst, frame):
        key = (src, frame["ver"]); seen = self.seen[dst].get(key)
        if seen is not None and self.t - seen < 2 * self.cfg["advert"]: return                  # one advert per source and version is used
        self.seen[dst][key] = self.t
        link = 1.0 if src == GATEWAY else self.etx(dst, src)
        total = link + frame["cost"]; via = frame["via"]; r = self.rt[dst]
        cand = dict(parent=src, cost=total, ver=frame["ver"], via=via, t=self.t, backup=None, bcost=None, bvia=None)
        if r is None or not self.valid(dst) or frame["ver"] > r["ver"]:
            self.rt[dst] = cand; return
        if frame["ver"] < r["ver"]: return
        if src == r["parent"]:
            if total <= r["cost"]: r["cost"] = total; r["t"] = self.t
            return
        if total < r["cost"] - 1e-9:
            old = dict(parent=r["parent"], cost=r["cost"], via=r["via"])
            cand["t"] = self.t
            if not (self.indep_backup and old["via"] == src): cand["backup"], cand["bcost"], cand["bvia"] = old["parent"], old["cost"], old["via"]
            elif r["backup"] is not None and not (self.indep_backup and r["bvia"] == src): cand["backup"], cand["bcost"], cand["bvia"] = r["backup"], r["bcost"], r["bvia"]
            self.rt[dst] = cand
        else:
            if self.indep_backup and via == r["parent"]: return            # a neighbour that routes through our own parent is no use as a backup
            if r["backup"] is None or total < r["bcost"]: r["backup"], r["bcost"], r["bvia"] = src, total, via
    def choose_parent(self, node):
        if node == GATEWAY or not self.valid(node): return None, None
        r = self.rt[node]; dead = {k for k, until in self.known_dead[node].items() if until > self.t}
        p, b = r["parent"], r["backup"]
        if p in dead: p, b = (b if b not in dead else None), None
        return p, b
    def on_hop_timeout(self, key, attempt, frame):
        node = key[0]
        if self.pending.get(key) != attempt or node in self.dead: return
        if attempt < M.HOP_RETRIES:
            self.hop_retx += 1; self.enqueue(node, dict(frame)); return
        del self.pending[key]; self.hop_fail += 1
        self.known_dead[node][frame["to"]] = self.t + 120.0
        r = self.rt[node]
        if r is not None and r["parent"] == frame["to"]:
            if self.cfg["backup"] and r["backup"] is not None:
                self.rt[node] = dict(parent=r["backup"], cost=r["bcost"], ver=r["ver"], via=r["bvia"], t=r["t"], backup=None, bcost=None, bvia=None)
                f = dict(frame); f["to"] = r["backup"]; self.enqueue(node, f); return
            self.rt[node] = None
        else:
            p, _ = self.choose_parent(node)
            if p is not None:
                f = dict(frame); f["to"] = p; self.enqueue(node, f); return
        self.held[node].append((self.t, frame)); self.at(self.t + self.rng.uniform(0, self.cfg["advert"]), "release", node)


class DVFixSim(DVSim):
    indep_backup = True
    fast_boot = True



M.AIR["rreq"] = M.airtime(20); M.AIR["rrep"] = M.airtime(18)       # 12-byte header + 6 / 4 bytes of payload + 2 CRC


class AODVSim(Counting):
    """On-demand routing in the style of AODV / HWMP, which is what the LoRa mesh papers adapt (Lundell et al. 2018, and the AODV-like baseline of the PPO routing paper):
    a node with no route to the gateway floods a route request, the gateway answers along the reverse path, routes are kept 30 minutes and refreshed by use, and a failed hop erases the
    route and starts a new discovery. Hop count is the metric. Only the gateway answers (no intermediate replies). No end-to-end receipt."""
    LIFETIME = 1800.0
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.route = [None] * (self.n + 1); self.rev = [dict() for _ in range(self.n + 1)]; self.rseen = [set() for _ in range(self.n + 1)]; self.rid = 0; self.disc = [None] * (self.n + 1)
    def on_advert(self, node): return
    def route_of(self, node):
        r = self.route[node]
        return r if r is not None and self.t - r["t"] <= self.LIFETIME else None
    def choose_parent(self, node):
        r = self.route_of(node); return (r["nh"] if r else None), None
    def send_data(self, node, frame):
        r = self.route_of(node)
        if r:
            r["t"] = self.t; f = dict(frame); f["to"] = r["nh"]; self.enqueue(node, f); return
        self.held[node].append((self.t, frame)); self.start_discovery(node)
    def start_discovery(self, node, attempt=0):
        if self.disc[node] is not None and attempt == 0: return
        self.disc[node] = attempt; self.rid += 1; rid = self.rid
        self.rseen[node].add((node, rid))
        self.enqueue(node, dict(kind="rreq", origin=node, rid=rid, hops=0), droppable=True)
        self.at(self.t + 5.0 * (2 ** attempt), "disc_timeout", node, attempt)
    def on_disc_timeout(self, node, attempt):
        if node in self.dead or self.route_of(node) is not None or self.disc[node] != attempt: return
        if attempt >= 3: self.disc[node] = None; return
        self.start_discovery(node, attempt + 1)
    def on_txend(self, node, frame, start):
        kind = frame["kind"]
        if kind == "rreq":
            for b in self.nbr[node]:
                if b not in self.dead and self.received(node, b, start, self.t): self.rx_rreq(node, b, frame)
            return
        if kind == "rrep":
            dst = frame["to"]
            if dst not in self.dead and self.received(node, dst, start, self.t): self.rx_rrep(node, dst, frame)
            return
        return super().on_txend(node, frame, start)
    def rx_rreq(self, src, dst, frame):
        key = (frame["origin"], frame["rid"])
        if key in self.rseen[dst]: return
        self.rseen[dst].add(key)
        self.rev[dst][frame["origin"]] = (src, frame["hops"] + 1)
        if dst == GATEWAY:
            self.enqueue(GATEWAY, dict(kind="rrep", to=src, origin=frame["origin"], hops=0), droppable=True); return
        self.enqueue(dst, dict(frame, hops=frame["hops"] + 1), droppable=True)
    def rx_rrep(self, src, dst, frame):
        self.route[dst] = dict(nh=src, hops=frame["hops"] + 1, t=self.t)
        if dst == frame["origin"]:
            self.disc[dst] = None; held, self.held[dst] = self.held[dst], []
            for (t0, f) in held:
                if self.t - t0 <= M.DTN_LIFETIME: self.send_data(dst, f)
            return
        nxt = self.rev[dst].get(frame["origin"])
        if nxt: self.enqueue(dst, dict(frame, to=nxt[0], hops=frame["hops"] + 1), droppable=True)
    def on_hop_timeout(self, key, attempt, frame):
        node = key[0]
        if self.pending.get(key) != attempt or node in self.dead: return
        if attempt < M.HOP_RETRIES:
            self.hop_retx += 1; self.enqueue(node, dict(frame)); return
        del self.pending[key]; self.hop_fail += 1
        if self.route[node] is not None and self.route[node]["nh"] == frame["to"]: self.route[node] = None
        self.held[node].append((self.t, frame)); self.start_discovery(node)


class RandomSim(Counting):
    """The 'random routing' baseline of the PPO routing paper: each packet goes to a random neighbour that is closer to the gateway. The costs are those known at install (frozen,
    free of any advert traffic, never repaired by an oracle); a node only learns locally that a neighbour failed."""
    frozen = False
    def __init__(self, *a, **k):
        super().__init__(*a, **k); self.frozen = True
    def compute_tree(self, exclude=lambda a, b: False):
        if not self.frozen: super().compute_tree(exclude)
    def choose_parent(self, node):
        bad = {k for k, until in self.known_dead[node].items() if until > self.t}
        c = [j for j in self.nbr[node] if self.cost[j] < self.cost[node] and j not in bad]
        return (self.rng.choice(c) if c else None), None


class StaticHopSim(Counting):
    """A tree fixed at install, built by hop count (the 'shortest path' baseline): the parent is the neighbour with the fewest hops to the gateway."""
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        hops = [math.inf] * (self.n + 1); hops[GATEWAY] = 0; frontier = [GATEWAY]
        while frontier:
            nxt = []
            for u in frontier:
                for v in self.nbr[u]:
                    if hops[v] == math.inf: hops[v] = hops[u] + 1; nxt.append(v)
            frontier = nxt
        self.fixed = [None] * (self.n + 1)
        for node in range(1, self.n + 1):
            c = [j for j in self.nbr[node] if hops[j] < hops[node]]
            self.fixed[node] = max(c, key=lambda j: self.rx[node][j]) if c else None
    def choose_parent(self, node): return self.fixed[node], None


class DVHopSim(DVFixSim):
    """Same as dv+fix but the route cost is the number of hops (every usable link costs 1), the usual alternative to ETX."""
    def etx(self, a, b): return 1.0


ALGOS = {
    "flood":   (FloodSim,  dict(lbt=True,  e2e=False, backup=False, advert=1e9)),
    "static":  (StaticSim, dict(lbt=True,  e2e=True,  backup=False, advert=1e9)),
    "oracle":  (OracleSim, M.CONFIGS["full"]),
    "dv":      (DVSim,     M.CONFIGS["full"]),
    "dv+fix":  (DVFixSim,  M.CONFIGS["full"]),
    "dv+fix-hops": (DVHopSim, M.CONFIGS["full"]),
    "aodv":    (AODVSim,   dict(lbt=True, e2e=False, backup=False, advert=1e9)),
    "random":  (RandomSim, dict(lbt=True, e2e=False, backup=False, advert=1e9)),
    "static-hops": (StaticHopSim, dict(lbt=True, e2e=False, backup=False, advert=1e9)),
    "dv-300":  (DVSim,     M.CONFIGS["full-300"]),
    "dv+fix-300": (DVFixSim, M.CONFIGS["full-300"]),
}


def one(args):
    algo, n, interval, seed, fail, dur = args
    cls, cfg = ALGOS[algo]
    try: return (algo, n, interval, fail), cls(n, interval, cfg, seed, dur, fail).run()
    except RuntimeError: return (algo, n, interval, fail), None


def main():
    quick = len(sys.argv) > 1 and sys.argv[1] == "quick"
    sizes = (20, 40) if quick else (20, 40, 80); seeds = range(1, 3) if quick else range(1, 11); intervals = (600,) if quick else (600, 60); dur = 1800.0 if quick else 3600.0
    jobs = [(a, n, iv, s, fail, dur) for fail in (False, True) for iv in intervals for n in sizes for a in ALGOS for s in seeds]
    groups = {}
    with cf.ProcessPoolExecutor() as pool:
        for key, r in pool.map(one, jobs, chunksize=2):
            if r is not None: groups.setdefault(key, []).append(r)
    mean = lambda xs: statistics.mean(x for x in xs if x is not None) if any(x is not None for x in xs) else None
    se = lambda xs: (statistics.stdev([x for x in xs if x is not None]) / math.sqrt(len([x for x in xs if x is not None]))) if len([x for x in xs if x is not None]) > 1 else 0.0
    rows = []
    f = lambda v: "  -  " if v is None else f"{v * 100:5.1f}"
    for fail in (False, True):
        for iv in intervals:
            print(f"\n{'RELAY DIES HALF WAY' if fail else 'no failure'}, one report per {iv} s per station:")
            print(f"  {'stations':>8} {'algorithm':<10} {'delivered':>14} {'alerts':>7} {'within 60 s':>17} {'median s':>8} {'p95 s':>7} {'tx / delivered':>14} {'channel busy':>12}   (within 60 s = first 5 min after boot{', after the relay dies' if fail else ''})")
            for n in sizes:
                for a in ALGOS:
                    rs = groups.get((a, n, iv, fail), [])
                    if not rs: print(f"  {n:>8} {a:<10}   (no connected layout)"); continue
                    agg = {k: mean([r.get(k) for r in rs]) for k in rs[0]}; agg.update(algo=a, n=n, interval_s=iv, relay_failure=fail, runs=len(rs), se_pdr=se([r.get("pdr") for r in rs]), se_60=se([r.get("fail_60s" if fail else "cold_60s") for r in rs])); rows.append(agg)
                    print(f"  {n:>8} {a:<10} {f(agg['pdr']):>8}% ±{agg['se_pdr'] * 100:3.1f} {f(agg['pdr_alert']):>6}% {f(agg['fail_60s'] if fail else agg['cold_60s']):>10}% ±{agg['se_60'] * 100:3.1f} {agg['lat_med'] or 0:>8.1f} {agg['lat_p95'] or 0:>7.1f} {agg['tx_per_delivered']:>14.2f} {f(agg['air_busy']):>11}%")
    json.dump(rows, open("routing_compare_quick.json" if quick else "routing_compare.json", "w"), indent=1)


if __name__ == "__main__":
    main()
