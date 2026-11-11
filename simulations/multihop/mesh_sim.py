"""Discrete-event simulation of many TerraWatch stations reporting to one gateway over a multi-hop LoRa tree.

It models the radio (path loss, shadowing, fading, capture, half-duplex), the collection tree (lowest-ETX parent, backup parent), the hop-by-hop ACK
with retries and the finite 8-packet queue of the firmware, and four configurations that differ in what this work adds:

  baseline  no listen-before-talk, no random delay, route adverts every 15 s, no end-to-end receipt, no backup parent
  +lbt      random delay before the first attempt and listen-before-talk with exponential back-off
  +e2e      the origin keeps each packet until the gateway's receipt arrives and resends after 12/20/30 s
  full      +backup parent (instant failover) and adverts every 60 s
  full-300  the same with adverts every 300 s (route lifetime would have to grow to match)

Run:  python3 mesh_sim.py            (full table, a few minutes)     python3 mesh_sim.py quick   (small, ~1 min)
"""
import heapq, itertools, json, math, random, statistics, sys

# ---- radio ----------------------------------------------------------------------------------------------------------------------------------------
BW, SF = 125e3, 7
PTX, NOISE, SNR_MIN = 14.0, -117.0, -7.5
SENS = NOISE + SNR_MIN            # -124.5 dBm: weakest frame that decodes
CAD_THRESHOLD = -122.0            # weakest signal listen-before-talk notices
CAPTURE_DB = 6.0
EXTRA_LOSS = 20.0                 # foliage and terrain on top of free space; 433 MHz
SHADOW_SIGMA, FADE_SIGMA = 4.0, 2.0
SIZES = {"data": 29, "ack": 16, "advert": 20, "e2e": 16}      # protocol v5: 12-byte header + payload (15 telemetry, 2 ack, 6 advert, 2 receipt) + 2-byte CRC. Before 2026-11-11 these were 1 byte short (the v4 header of 11 bytes).

def airtime(nbytes):
    tsym = 2 ** SF / BW
    nsym = 8 + max(math.ceil((8 * nbytes - 4 * SF + 28 + 16) / (4 * SF)) * 5, 0)
    return (8 + 4.25) * tsym + nsym * tsym

AIR = {k: airtime(v) for k, v in SIZES.items()}

# ---- firmware parameters (node-firmware/include/Constants.h and Config.h) ------------------------------------------------------------------------------
QUEUE_CAP, HOP_ACK_TIMEOUT, HOP_RETRIES = 8, 2.5, 3
DUP_EXPIRY, DTN_LIFETIME = 10.0, 300.0
JITTER_MAX, LBT_BASE, LBT_MAX = 0.060, 0.040, 6
E2E_AFTER = (12.0, 20.0, 30.0)
GATEWAY = 0
ALERT_EVERY = 1800.0              # mean seconds between sound alerts at one station (chainsaw, blast): rare, and the ones that must arrive

CONFIGS = {
    "baseline": dict(lbt=False, e2e=False, backup=False, advert=15.0),
    "+lbt":     dict(lbt=True,  e2e=False, backup=False, advert=15.0),
    "+e2e":     dict(lbt=True,  e2e=True,  backup=False, advert=15.0),
    "full":     dict(lbt=True,  e2e=True,  backup=True,  advert=60.0),
    "full-300": dict(lbt=True,  e2e=True,  backup=True,  advert=300.0),   # what the backup parent makes possible: adverts only every 5 minutes
}


class Sim:
    def __init__(self, n, interval, cfg, seed, duration=3600.0, fail=False):
        self.rng = random.Random(seed)
        self.n, self.interval, self.cfg, self.T, self.fail = n, interval, cfg, duration, fail
        self.place()
        self.t, self.ev, self.cnt = 0.0, [], itertools.count()
        self.queue = [[] for _ in range(n + 1)]
        self.armed = [False] * (n + 1)
        self.lbt_n = [0] * (n + 1)
        self.busy_until = [0.0] * (n + 1)
        self.scheduled = [False] * (n + 1)
        self.txs = []                       # recent transmissions (start, end, node, power-by-listener cache)
        self.dup = {}                       # (node, origin, pid) -> time seen
        self.dead = set()
        self.cut_off = set()
        self.known_dead = [dict() for _ in range(n + 1)]   # node -> {neighbour: until}
        self.route_ready = [0.0] * (n + 1)
        self.held = [[] for _ in range(n + 1)]
        self.pending = {}                   # (node, origin, pid) -> retries (hop ACK wait)
        self.e2e = {}                       # (origin, pid) -> retries
        self.gen, self.delivered, self.confirmed, self.gave_up = {}, {}, set(), 0
        self.alerts = set()
        self.air_data = self.air_ctl = 0.0
        self.hop_retx = self.queue_drops = self.lbt_backoffs = self.hop_fail = 0
        for _ in range(50):                  # a forest deployment is planned to be connected: redraw the layout if a station is cut off
            self.compute_tree()
            if all(not math.isinf(c) for c in self.cost[1:]): break
            self.place()
        else:
            raise RuntimeError("no connected layout")

    # ---- topology -----------------------------------------------------------------------------------------------------------------------------
    def place(self):
        side = max(2, math.ceil(math.sqrt(self.n)))
        spacing = 1.4                                  # km between neighbouring stations, jittered
        self.pos = [(0.0, 0.0)]                        # gateway in the corner of the forest
        cells = [(i, j) for i in range(side + 1) for j in range(side + 1) if (i, j) != (0, 0)]
        self.rng.shuffle(cells)
        for i, j in cells[:self.n]:
            self.pos.append(((i + self.rng.uniform(-.3, .3)) * spacing, (j + self.rng.uniform(-.3, .3)) * spacing))
        N = self.n + 1
        self.rx = [[None] * N for _ in range(N)]
        for a in range(N):
            for b in range(a + 1, N):
                d = max(0.05, math.dist(self.pos[a], self.pos[b]))
                pl = 32.44 + 20 * math.log10(433) + 20 * math.log10(d) + EXTRA_LOSS + 15 * math.log10(max(d, 0.1) / 0.1)   # exponent 3.5 overall
                p = PTX - pl - self.rng.gauss(0, SHADOW_SIGMA)
                self.rx[a][b] = self.rx[b][a] = p
        self.hear = [[b for b in range(N) if b != a and self.rx[a][b] >= CAD_THRESHOLD] for a in range(N)]
        self.nbr = [[b for b in range(N) if b != a and self.rx[a][b] >= SENS + 3] for a in range(N)]

    def prr(self, a, b):
        z = (self.rx[a][b] - SENS) / FADE_SIGMA
        return 0.5 * (1 + math.erf(z / math.sqrt(2)))

    def etx(self, a, b):
        p = self.prr(a, b)
        return 1 / max(p * p, 0.02)

    def compute_tree(self, exclude=lambda a, b: False):
        N = self.n + 1
        cost = [math.inf] * N; cost[GATEWAY] = 0.0
        h = [(0.0, GATEWAY)]
        while h:
            c, u = heapq.heappop(h)
            if c > cost[u]: continue
            for v in self.nbr[u]:
                if v in self.dead or exclude(v, u): continue
                nc = c + self.etx(v, u)
                if nc < cost[v]: cost[v] = nc; heapq.heappush(h, (nc, v))
        self.cost = cost

    def choose_parent(self, node):
        """(parent, backup): neighbours that are closer to the gateway, best total cost first; neighbours this node saw fail are skipped."""
        bad = {k for k, until in self.known_dead[node].items() if until > self.t}
        cands = sorted((self.etx(node, j) + self.cost[j], j) for j in self.nbr[node] if self.cost[j] < self.cost[node] and j not in bad)
        return (cands[0][1] if cands else None), (cands[1][1] if len(cands) > 1 else None)

    # ---- event plumbing -----------------------------------------------------------------------------------------------------------------------
    def at(self, when, kind, *args):
        heapq.heappush(self.ev, (when, next(self.cnt), kind, args))

    def run(self):
        for node in range(1, self.n + 1):
            self.at(self.rng.uniform(0, self.interval), "gen", node)
            self.at(self.rng.expovariate(1 / ALERT_EVERY), "alert", node)
            self.at(self.rng.uniform(0, self.cfg["advert"]), "advert", node)
        if self.fail:
            self.at(self.T / 2, "kill")
        while self.ev:
            when, _, kind, args = heapq.heappop(self.ev)
            if when > self.T + 120: break
            self.t = when
            getattr(self, "on_" + kind)(*args)
        return self.report()

    # ---- traffic ------------------------------------------------------------------------------------------------------------------------------
    def on_gen(self, node):
        if node in self.dead or self.t > self.T: return
        pid = len(self.gen)
        self.gen[(node, pid)] = self.t
        self.send_data(node, dict(kind="data", origin=node, pid=pid, born=self.t, path=[node], hops=0))
        self.at(self.t + self.interval, "gen", node)

    def on_alert(self, node):
        if node in self.dead or self.t > self.T: return
        pid = len(self.gen)
        self.gen[(node, pid)] = self.t
        self.alerts.add((node, pid))
        self.send_data(node, dict(kind="data", origin=node, pid=pid, born=self.t, path=[node], hops=0, alert=True))
        self.at(self.t + self.rng.expovariate(1 / ALERT_EVERY), "alert", node)

    def on_advert(self, node):
        if node in self.dead or self.t > self.T: return
        self.enqueue(node, dict(kind="advert", origin=node), front=False, droppable=True)
        self.at(self.t + self.cfg["advert"], "advert", node)

    def on_kill(self):
        # take out the relay that carries the most stations (not the one next to the gateway only if that is all there is)
        load = {}
        for node in range(1, self.n + 1):
            u, seen = node, 0
            while u != GATEWAY and seen < 50:
                p = self.choose_parent(u)[0]
                if p is None: break
                if p != GATEWAY: load[p] = load.get(p, 0) + 1
                u, seen = p, seen + 1
        # the relay carrying the most stations whose loss still leaves every other station a path (otherwise the test measures geography, not protocol)
        for victim in sorted(load, key=load.get, reverse=True):
            self.dead = {victim}
            if len(self.reachable()) == self.n - 1: break
        else:
            victim = max(load, key=load.get) if load else None
            self.dead = {victim} if victim else set()
        if victim:
            self.queue[victim].clear(); self.victim = victim
            self.cut_off = set(range(1, self.n + 1)) - self.dead - self.reachable()
            # nodes keep believing the old tree: they only learn the relay is gone from failed ACKs, then from their neighbours' next adverts

    def reachable(self):
        seen, stack = {GATEWAY}, [GATEWAY]
        while stack:
            u = stack.pop()
            for v in self.nbr[u]:
                if v not in seen and v not in self.dead: seen.add(v); stack.append(v)
        return seen - {GATEWAY}

    def enqueue(self, node, frame, front=False, droppable=False):
        q = self.queue[node]
        if len(q) >= QUEUE_CAP:
            if frame["kind"] == "data": self.queue_drops += 1
            return False
        q.insert(0, frame) if front else q.append(frame)
        self.kick(node)
        return True

    # ---- MAC ----------------------------------------------------------------------------------------------------------------------------------
    def kick(self, node):
        if self.scheduled[node] or node in self.dead or not self.queue[node]: return
        self.scheduled[node] = True
        if not self.armed[node]:
            self.armed[node] = True; self.lbt_n[node] = 0
            wait = self.rng.uniform(0, JITTER_MAX) if self.cfg["lbt"] else 0.0
            self.at(max(self.t, self.busy_until[node]) + wait, "try", node)
        else:
            self.at(max(self.t, self.busy_until[node]), "try", node)

    def sensed_busy(self, node):
        for (s, e, m, _f) in self.txs:
            if m != node and s + 0.002 <= self.t < e and self.rx[m][node] >= CAD_THRESHOLD: return True
        return False

    def on_try(self, node):
        self.scheduled[node] = False
        if node in self.dead or not self.queue[node]: return
        if self.t < self.busy_until[node]:
            self.kick(node); return
        if self.cfg["lbt"] and self.lbt_n[node] < LBT_MAX and self.sensed_busy(node):
            window = LBT_BASE * (2 ** self.lbt_n[node]); self.lbt_n[node] += 1; self.lbt_backoffs += 1
            self.scheduled[node] = True
            self.at(self.t + self.rng.uniform(0.001, window), "try", node)
            return
        frame = self.queue[node].pop(0)
        self.armed[node] = False
        self.transmit(node, frame)
        self.kick(node)

    def transmit(self, node, frame):
        air = AIR[frame["kind"]]
        end = self.t + air
        self.busy_until[node] = end
        self.txs = [x for x in self.txs if x[1] > self.t - 1.0]
        self.txs.append((self.t, end, node, frame))
        if frame["kind"] == "data": self.air_data += air
        else: self.air_ctl += air
        self.at(end, "txend", node, frame, self.t)

    def on_txend(self, node, frame, start):
        end = self.t
        kind = frame["kind"]
        # who can decode it: the addressee only for unicast; adverts matter just as channel load
        if kind == "advert": return
        dst = frame["to"]
        if dst in self.dead or not self.received(node, dst, start, end): 
            if kind == "data": self.arm_hop_timer(node, frame)
            return
        getattr(self, "rx_" + kind)(node, dst, frame)
        if kind == "data": self.arm_hop_timer(node, frame)

    def received(self, src, dst, start, end):
        if self.rx[src][dst] + self.rng.gauss(0, FADE_SIGMA) < SENS: return False
        sig = self.rx[src][dst]
        for (s, e, m, _f) in self.txs:
            if m == src or e <= start or s >= end: continue
            if m == dst: return False                                     # half-duplex: it was talking
            if self.rx[m][dst] + self.rng.gauss(0, FADE_SIGMA) > sig - CAPTURE_DB and self.rx[m][dst] > SENS - 6: return False
        return True

    # ---- hop-by-hop reliability ---------------------------------------------------------------------------------------------------------------
    def arm_hop_timer(self, node, frame):
        key = (node, frame["origin"], frame["pid"])
        self.pending[key] = self.pending.get(key, -1) + 1
        self.at(self.t + HOP_ACK_TIMEOUT, "hop_timeout", key, self.pending[key], frame)

    def on_hop_timeout(self, key, attempt, frame):
        node = key[0]
        if self.pending.get(key) != attempt or node in self.dead: return          # acknowledged meanwhile
        if attempt < HOP_RETRIES:
            self.hop_retx += 1
            self.enqueue(node, dict(frame))
            return
        del self.pending[key]
        self.hop_fail += 1
        self.known_dead[node][frame["to"]] = self.t + 120.0
        self.compute_tree()
        if self.cfg["backup"]:
            p, _ = self.choose_parent(node)
            if p is not None:
                f = dict(frame); f["to"] = p; self.enqueue(node, f); return
        self.held[node].append((self.t, frame))
        self.route_ready[node] = self.t + self.rng.uniform(0, self.cfg["advert"])  # wait for a neighbour's next advert
        self.at(self.route_ready[node], "release", node)

    def on_release(self, node):
        keep = []
        for (t0, frame) in self.held[node]:
            if self.t - t0 > DTN_LIFETIME: continue
            p, _ = self.choose_parent(node)
            if p is None: keep.append((t0, frame)); continue
            f = dict(frame); f["to"] = p; self.enqueue(node, f)
        self.held[node] = keep
        if keep: self.at(self.t + self.cfg["advert"], "release", node)

    def send_data(self, node, frame):
        p, _ = self.choose_parent(node)
        if p is None:
            self.held[node].append((self.t, frame)); self.at(self.t + self.cfg["advert"], "release", node); return
        f = dict(frame); f["to"] = p
        self.enqueue(node, f)

    # ---- receive handlers ---------------------------------------------------------------------------------------------------------------------
    def rx_data(self, src, dst, frame):
        key = (dst, frame["origin"], frame["pid"])
        self.enqueue(dst, dict(kind="ack", to=src, origin=frame["origin"], pid=frame["pid"]), front=True)
        seen = self.dup.get(key)
        dup = seen is not None and self.t - seen < (DUP_EXPIRY if dst != GATEWAY else 120.0)
        self.dup[key] = self.t if not dup else seen
        if dst == GATEWAY:
            first = (frame["origin"], frame["pid"]) not in self.delivered
            if first: self.delivered[(frame["origin"], frame["pid"])] = self.t
            if self.cfg["e2e"] and src != frame["origin"]:
                path = list(frame["path"]) + [dst]
                rev = path[::-1]
                self.enqueue(GATEWAY, dict(kind="e2e", to=rev[1], route=rev[1:], origin=frame["origin"], pid=frame["pid"]))
            return
        if dup: return
        f = dict(frame); f["path"] = frame["path"] + [dst]; f["hops"] = frame["hops"] + 1
        self.send_data(dst, f)

    def rx_ack(self, src, dst, frame):
        key = (dst, frame["origin"], frame["pid"])
        self.pending.pop(key, None)
        if src == GATEWAY and frame["origin"] == dst:
            self.confirmed.add((dst, frame["pid"]))               # the gateway itself acknowledged: that is the receipt

    def rx_e2e(self, src, dst, frame):
        route = frame["route"]
        if dst == frame["origin"]:
            self.confirmed.add((dst, frame["pid"])); return
        if len(route) < 2: return
        nxt = route[1]
        f = dict(frame); f["to"] = nxt; f["route"] = route[1:]
        self.enqueue(dst, f)

    def on_e2e_timer(self, key, attempt):
        origin = key[0]
        if key in self.confirmed or origin in self.dead or self.e2e.get(key) != attempt: return
        if attempt >= len(E2E_AFTER): self.gave_up += 1; return
        self.e2e[key] = attempt + 1
        self.send_data(origin, dict(kind="data", origin=origin, pid=key[1], born=self.gen[key], path=[origin], hops=0, alert=True))
        self.at(self.t + E2E_AFTER[min(attempt + 1, len(E2E_AFTER) - 1)], "e2e_timer", key, attempt + 1)

    # first transmission of an own packet starts the end-to-end clock
    def enqueue_hook(self):
        pass

    # ---- results ------------------------------------------------------------------------------------------------------------------------------
    def report(self):
        gen_after = {k: v for k, v in self.gen.items() if k[0] not in self.dead and k[0] not in self.cut_off}
        window = {k: v for k, v in gen_after.items() if v >= (self.T / 2 if self.fail else 0.0)}
        dl = [self.delivered[k] - self.gen[k] for k in window if k in self.delivered]
        total = max(len(window), 1)
        out = dict(n=self.n, generated=len(window), pdr=len(dl) / total,
                   lat_med=statistics.median(dl) if dl else None, lat_p95=sorted(dl)[int(.95 * (len(dl) - 1))] if dl else None,
                   hop_retx=self.hop_retx / total, queue_drops=self.queue_drops / total, lbt_backoffs=self.lbt_backoffs / total,
                   air_busy=(self.air_data + self.air_ctl) / self.T, air_per_node=(self.air_data + self.air_ctl) / self.T / self.n,
                   ctl_share=self.air_ctl / max(self.air_data + self.air_ctl, 1e-9))
        al = [k for k in window if k in self.alerts]
        out["alerts"] = len(al)
        out["pdr_alert"] = sum(1 for k in al if k in self.delivered) / len(al) if al else None
        if self.cfg["e2e"]:
            out["alert_known_lost"] = sum(1 for k in al if k not in self.delivered and self.e2e.get(k, 0) >= len(E2E_AFTER)) / len(al) if al else None
        return out


# the end-to-end clock: start it when an own data packet leaves the radio the first time
_orig_transmit = Sim.transmit
def _transmit(self, node, frame):
    if self.cfg["e2e"] and frame["kind"] == "data" and frame.get("alert") and frame["origin"] == node and (node, frame["pid"]) not in self.e2e:
        self.e2e[(node, frame["pid"])] = 0
        self.at(self.t + E2E_AFTER[0], "e2e_timer", (node, frame["pid"]), 0)
    _orig_transmit(self, node, frame)
Sim.transmit = _transmit


def one(args):
    n, interval, name, seed, dur, fail = args
    return (n, interval, name, fail), Sim(n, interval, CONFIGS[name], seed, dur, fail).run()


def main():
    import concurrent.futures as cf
    quick = len(sys.argv) > 1 and sys.argv[1] == "quick"
    sizes = (10, 30) if quick else (10, 20, 40, 80)
    intervals = (60,) if quick else (60, 600)
    seeds = (1, 2) if quick else (1, 2, 3, 4, 5, 6)
    dur = 1800.0 if quick else 7200.0
    jobs = [(n, iv, name, s, dur, fail) for fail in (False, True) for iv in intervals for n in sizes for name in CONFIGS for s in seeds]
    groups = {}
    with cf.ProcessPoolExecutor() as pool:
        for key, r in pool.map(one, jobs, chunksize=4): groups.setdefault(key, []).append(r)
    rows = []
    mean = lambda xs: statistics.mean(x for x in xs if x is not None) if any(x is not None for x in xs) else None
    for (n, iv, name, fail), rs in sorted(groups.items(), key=lambda kv: (kv[0][3], kv[0][1], kv[0][0], list(CONFIGS).index(kv[0][2]))):
        agg = {k: mean([r.get(k) for r in rs]) for k in rs[0] if k != "n"}
        agg.update(config=name, n=n, interval_s=iv, relay_failure=fail)
        rows.append(agg)
        f = lambda v, p=1: "  -  " if v is None else f"{v * 100:5.{p}f}"
        print(f"{'FAIL' if fail else 'ok  '} T={iv:>3}s n={n:>2} {name:<8} telemetry {f(agg['pdr'])}%  alerts {f(agg['pdr_alert'])}%  lat_med={agg['lat_med'] or 0:5.1f}s  "
              f"hop_retx/pkt={agg['hop_retx']:.2f} drops/pkt={agg['queue_drops']:.2f} busy={agg['air_busy']*100:5.1f}% ctl={agg['ctl_share']*100:4.1f}%")
    path = "results_quick.json" if quick else "results.json"
    json.dump(rows, open(path, "w"), indent=1)
    print("saved", path)


if __name__ == "__main__":
    main()
