"""Radio energy of the routing designs, from the simulator's transmissions and the receiver duty each design needs.
Currents (SX1278 module datasheets, typical, to be confirmed on a bench): transmit 90 mA at 17 dBm (PA_BOOST, the library default the firmware uses), receive 10.8 mA (LnaBoost off).
Only the radio is counted (no ESP32, PM fan, sensors). Run: python3 energy_estimate.py"""
import concurrent.futures as cf, statistics
import mesh_sim as M
import routing_compare as R
import gateway_placement as G

I_TX, I_RX = 90.0, 10.8          # mA
DAY = 86400.0


class Track:
    def __init__(self, *a, **k):
        super().__init__(*a, **k); self.txtime = [0.0] * (self.n + 1); self.fwd = set()
    def transmit(self, node, frame):
        self.txtime[node] += M.AIR[frame["kind"]]; super().transmit(node, frame)
    def send_data(self, node, frame):
        if frame.get("origin") != node: self.fwd.add(node)
        super().send_data(node, frame)


class TrackDV(Track, R.DVFixSim): pass
class TrackStatic(Track, R.StaticSim): pass
class TrackFlood(Track, R.FloodSim):
    def on_txend(self, node, frame, start):
        super().on_txend(node, frame, start)


def one(args):
    algo, layout, n, seed = args
    M.Sim.place = G.place_with(G.LAYOUTS[layout])
    cls = {"dv+fix": TrackDV, "static": TrackStatic}[algo]; _, cfg = R.ALGOS[algo]
    try: sim = cls(n, 600, cfg, seed, 3600.0, False); sim.run()
    except RuntimeError: return None
    T = sim.T; nodes = range(1, n + 1)
    relays = [i for i in nodes if i in sim.fwd]; leaves = [i for i in nodes if i not in sim.fwd]
    tx_mah = lambda i: sim.txtime[i] / T * DAY * I_TX / 3600.0
    return dict(algo=algo, layout=layout, n=n, relay_share=len(relays) / n, tx_relay=statistics.mean(tx_mah(i) for i in relays) if relays else 0.0, tx_leaf=statistics.mean(tx_mah(i) for i in leaves) if leaves else 0.0)


def main():
    jobs = [(a, l, n, s) for a in ("dv+fix", "static") for l in ("corner", "centre") for n in (20, 40, 80) for s in range(1, 11)]
    groups = {}
    with cf.ProcessPoolExecutor() as pool:
        for r in pool.map(one, jobs, chunksize=2):
            if r: groups.setdefault((r["algo"], r["layout"], r["n"]), []).append(r)
    rx_always = I_RX * 24.0
    print(f"receiver always on: {rx_always:.0f} mAh/day (10.8 mA x 24 h). Transmit current 90 mA.")
    print(f"{'design':<8} {'gateway':<7} {'stations':>8} {'relays':>7} {'TX relay':>12} {'TX leaf':>12}   mAh per day, radio transmit only")
    for (a, l, n), rs in sorted(groups.items()):
        m = lambda k: statistics.mean(r[k] for r in rs)
        print(f"{a:<8} {l:<7} {n:>8} {m('relay_share') * 100:6.0f}% {m('tx_relay'):10.2f} mAh {m('tx_leaf'):10.2f} mAh")
    print()
    print("Receive-side budget (mAh/day) for a station whose receiver is on a fraction f of the time:")
    for f in (1.0, 0.25, 0.08, 0.02): print(f"  f = {f:>5.2f}: {rx_always * f:6.1f} mAh/day  ->  3000 mAh battery, radio only: {3000 / (rx_always * f):6.1f} days")


if __name__ == "__main__":
    main()
