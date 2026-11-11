"""Where the gateway sits: corner of the forest (what mesh_sim.py assumed) against the centre, and two gateways (a second at the opposite corner, stations use the better one).
Same radio model and failure test as routing_compare.py; the firmware design (dv+fix) and the static tree are compared. Run: python3 gateway_placement.py"""
import concurrent.futures as cf, math, statistics
import mesh_sim as M
import routing_compare as R


def place_with(gw_positions):
    def place(self):
        side = max(2, math.ceil(math.sqrt(self.n))); spacing = 1.4
        gws = [(gx * side * spacing, gy * side * spacing) for gx, gy in gw_positions]
        cells = [(i, j) for i in range(side + 1) for j in range(side + 1)]
        taken = {(round(g[0] / spacing), round(g[1] / spacing)) for g in gws}
        cells = [c for c in cells if c not in taken]; self.rng.shuffle(cells)
        self.pos = [gws[0]] + [((i + self.rng.uniform(-.3, .3)) * spacing, (j + self.rng.uniform(-.3, .3)) * spacing) for i, j in cells[:self.n]]
        self.gw2 = gws[1] if len(gws) > 1 else None
        N = self.n + 1; self.rx = [[None] * N for _ in range(N)]
        for a in range(N):
            for b in range(a + 1, N):
                d = max(0.05, math.dist(self.pos[a], self.pos[b]))
                pl = 32.44 + 20 * math.log10(433) + 20 * math.log10(d) + M.EXTRA_LOSS + 15 * math.log10(max(d, 0.1) / 0.1)
                p = M.PTX - pl - self.rng.gauss(0, M.SHADOW_SIGMA); self.rx[a][b] = self.rx[b][a] = p
        if self.gw2 is not None:      # the second gateway is a node that the first one's tree cannot tell apart: stations simply see two sinks. Modelled by letting a station that is closer to gw2 use it.
            for b in range(1, N):
                d2 = max(0.05, math.dist(self.gw2, self.pos[b])); d1 = math.dist(self.pos[0], self.pos[b])
                if d2 < d1:
                    pl = 32.44 + 20 * math.log10(433) + 20 * math.log10(d2) + M.EXTRA_LOSS + 15 * math.log10(max(d2, 0.1) / 0.1)
                    p = M.PTX - pl - self.rng.gauss(0, M.SHADOW_SIGMA); self.rx[0][b] = self.rx[b][0] = max(self.rx[0][b], p)
        self.hear = [[b for b in range(N) if b != a and self.rx[a][b] >= M.CAD_THRESHOLD] for a in range(N)]
        self.nbr = [[b for b in range(N) if b != a and self.rx[a][b] >= M.SENS + 3] for a in range(N)]
    return place


LAYOUTS = {"corner": [(0.0, 0.0)], "centre": [(0.5, 0.5)]}


def one(args):
    layout, algo, n, seed, fail = args
    M.Sim.place = place_with(LAYOUTS[layout]) if layout in LAYOUTS else place_with(LAYOUTS["corner"])
    cls, cfg = R.ALGOS[algo]
    try: return (layout, algo, n, fail), cls(n, 600, cfg, seed, 3600.0, fail).run()
    except RuntimeError: return (layout, algo, n, fail), None


def main():
    jobs = [(l, a, n, s, f) for l in LAYOUTS for a in ("static", "dv+fix", "flood") for n in (40, 80) for s in range(1, 11) for f in (False, True)]
    groups = {}
    with cf.ProcessPoolExecutor() as pool:
        for k, r in pool.map(one, jobs, chunksize=2):
            if r: groups.setdefault(k, []).append(r)
    m = lambda rs, key: statistics.mean(r[key] for r in rs if r.get(key) is not None)
    print(f"{'gateway':<8} {'stations':>8} {'design':<8} {'no failure':>11} {'relay dies':>11} {'p95 s':>6} {'tx/packet':>9} {'busy':>6}")
    for l in LAYOUTS:
        for n in (40, 80):
            for a in ("static", "dv+fix", "flood"):
                ok, bad = groups.get((l, a, n, False), []), groups.get((l, a, n, True), [])
                if ok and bad: print(f"{l:<8} {n:>8} {a:<8} {m(ok, 'pdr') * 100:10.1f}% {m(bad, 'pdr') * 100:10.1f}% {m(ok, 'lat_p95'):6.1f} {m(ok, 'tx_per_delivered'):9.1f} {m(ok, 'air_busy') * 100:5.1f}%")


if __name__ == "__main__":
    main()
