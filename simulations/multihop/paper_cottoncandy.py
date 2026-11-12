"""Our design in the scenario of the CottonCandy paper (Wu and Liebeherr, University of Toronto; ns-3): 100 nodes on a 10x10 grid with the root at the centre,
SF7, 125 kHz, CR 4/5, 17 dBm, path loss 7.7 dB + 37.6 log10(d in m) (exponent 3.76, range 3.3 km), sensitivity -123 dBm, no shadowing,
one report per node per round, grid spacing 0.5 km (their 25 km2 case) and 2 km (their 324 km2 case). They report a delivery ratio above 90 % for every hop distance.
Ours differ in packet size (29 bytes with our 12-byte header against their 15), and in a burst round there is no TDMA: all nodes report within a window (60 s, 600 s or 3600 s).
Run: python3 paper_cottoncandy.py"""
import concurrent.futures as cf, math, statistics
import mesh_sim as M
import routing_compare as R

WARM = 300.0          # routes are built before the round, as in the paper (measurement starts after all nodes joined)


def place_grid(spacing):
    def place(self):
        side = 10; self.n = 100
        self.pos = [((side - 1) * spacing / 2, (side - 1) * spacing / 2)] + [(i * spacing, j * spacing) for i in range(side) for j in range(side)]
        N = self.n + 1; self.rx = [[None] * N for _ in range(N)]
        for a in range(N):
            for b in range(a + 1, N):
                d_m = max(1.0, math.dist(self.pos[a], self.pos[b]) * 1000)
                self.rx[a][b] = self.rx[b][a] = M.PTX - (7.7 + 37.6 * math.log10(d_m))
        self.hear = [[b for b in range(N) if b != a and self.rx[a][b] >= M.CAD_THRESHOLD] for a in range(N)]
        self.nbr = [[b for b in range(N) if b != a and self.rx[a][b] >= M.SENS + 3] for a in range(N)]
    return place


def one_round(self, node):                   # one report per node, inside the window, after the warm-up
    if self.t < WARM: return self.at(WARM + self.t, "gen", node)
    if node in self.dead or self.t > self.T: return
    pid = len(self.gen); self.gen[(node, pid)] = self.t
    self.send_data(node, dict(kind="data", origin=node, pid=pid, born=self.t, path=[node], hops=0))


def job(args):
    spacing, window, algo, seed = args
    M.PTX, M.SENS, M.SHADOW_SIGMA, M.ALERT_EVERY = 17.0, -123.0, 0.0, 1e12
    M.Sim.place = place_grid(spacing)
    cls, cfg = R.ALGOS[algo]
    sim = type("S", (cls,), {"on_gen": one_round})(100, window, cfg, seed, WARM + window + 600.0, False)
    return (spacing, window, algo), sim.run()


def main():
    algos = ("dv+fix", "static", "flood", "random")
    jobs = [(sp, w, a, s) for sp in (0.5, 2.0) for w in (60, 600, 3600) for a in algos for s in range(1, 9)]
    groups = {}
    with cf.ProcessPoolExecutor() as pool:
        for k, r in pool.map(job, jobs, chunksize=2): groups.setdefault(k, []).append(r)
    print("CottonCandy paper: delivery above 90 % for 100 nodes, grids of 0.5 km and 2 km. Ours, one report per node, 8 runs each:")
    print(f"{'spacing':>8} {'window':>7} {'design':<8} {'delivered':>10} {'lowest run':>10} {'tx/packet':>9} {'channel busy':>12}")
    for sp in (0.5, 2.0):
        for w in (60, 600, 3600):
            for a in algos:
                rs = groups[(sp, w, a)]; p = [r["pdr"] for r in rs]
                print(f"{sp:>6} km {w:>6}s {a:<8} {statistics.mean(p) * 100:9.1f}% {min(p) * 100:9.1f}% {statistics.mean(r['tx_per_delivered'] for r in rs):9.1f} {statistics.mean(r['air_busy'] for r in rs) * 100:11.1f}%")


if __name__ == "__main__":
    main()
