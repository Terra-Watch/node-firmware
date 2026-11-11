"""Cross-check of the collision and capture model of mesh_sim.py against LoRaSim 0.2.1 (Bor, Roedig, Voigt, Alonso, MSWiM 2016; https://github.com/mcbor/lorasim, CC BY 4.0).
Both simulate one gateway and N nodes sending 29-byte packets at SF7, 125 kHz, CR 4/5 with Poisson traffic, nodes uniform in a disc of 70 m (so received powers differ and capture matters),
the same path loss (LoRaSim's log-distance model, exponent 2.08, 127.41 dB at 40 m), sensitivity -126.5 dBm, capture threshold 6 dB, no acknowledgements, no retries.
LoRaSim "full collision" = its capture-effect model; "simple" = pure ALOHA (any overlap on the same channel and SF is a collision).
lorasim_tw.py is LoRaSim's loraDir.py converted to Python 3 with an experiment 6 for these settings and quieter output (the physics code is untouched).
Run: python3 cross_check_lorasim.py [path/to/python-with-simpy]"""
import concurrent.futures as cf, json, math, os, random, statistics, subprocess, sys
import mesh_sim as M

PY = sys.argv[1] if len(sys.argv) > 1 else "python3"
HERE = os.path.dirname(os.path.abspath(__file__))
MAXDIST = 70.0


def lorasim(args):
    n, interval_ms, full, seed = args
    env = dict(os.environ, SEED=str(seed), PLEN="29", MAXDIST=str(MAXDIST))
    out = subprocess.run([PY, os.path.join(HERE, "lorasim_tw.py"), str(n), str(interval_ms), "6", "3600000", str(full)], env=env, capture_output=True, text=True, timeout=1800).stdout.strip().splitlines()[-1]
    return json.loads(out)["der"]


# ---- our simulator under the same physics --------------------------------------------------------------------------------------------------------------
def lora_pl(d): return 127.41 + 10 * 2.08 * math.log10(max(d, 1.0) / 40.0)


class Star(M.Sim):
    def place(self):
        r = random.Random(self.rng.random())
        self.pos = [(0.0, 0.0)]
        for _ in range(self.n):
            a, b = r.random(), r.random()
            if b < a: a, b = b, a
            self.pos.append((b * MAXDIST * math.cos(2 * math.pi * a / b), b * MAXDIST * math.sin(2 * math.pi * a / b)))
        N = self.n + 1; self.rx = [[None] * N for _ in range(N)]
        for a in range(N):
            for b in range(a + 1, N):
                d = math.dist(self.pos[a], self.pos[b]); self.rx[a][b] = self.rx[b][a] = M.PTX - lora_pl(d)
        self.hear = [[b for b in range(N) if b != a and self.rx[a][b] >= M.CAD_THRESHOLD] for a in range(N)]
        self.nbr = [[b for b in range(N) if b != a and self.rx[a][b] >= M.SENS + 3] for a in range(N)]
    def on_gen(self, node):                      # Poisson traffic, as LoRaSim
        if node in self.dead or self.t > self.T: return
        pid = len(self.gen); self.gen[(node, pid)] = self.t
        self.send_data(node, dict(kind="data", origin=node, pid=pid, born=self.t, path=[node], hops=0))
        self.at(self.t + self.rng.expovariate(1 / self.interval), "gen", node)
    def enqueue(self, node, frame, front=False, droppable=False):
        if frame["kind"] == "ack": return False
        return super().enqueue(node, frame, front, droppable)
    def arm_hop_timer(self, node, frame): return          # no acknowledgements and no retries, as LoRaSim: a lost packet is simply lost


def ours(args):
    n, interval_s, full, seed = args
    M.SENS, M.SHADOW_SIGMA, M.FADE_SIGMA, M.HOP_RETRIES, M.CAPTURE_DB = -126.5, 0.0, 1e-9, 0, 6.0
    M.AIR = {k: M.airtime(v) for k, v in M.SIZES.items()}
    if not full: M.CAPTURE_DB = 1e9              # any overlap is a collision: pure ALOHA
    s = Star(n, interval_s, dict(lbt=False, e2e=False, backup=False, advert=1e12), seed, 3600.0, False)
    return s.run()["pdr"]


def main():
    cases = [(20, 60), (40, 60), (80, 60), (40, 20), (80, 20), (160, 20), (160, 10)]
    print(f"{'nodes':>5} {'interval':>8} | {'LoRaSim full':>13} {'ours full':>11} {'diff':>7} | {'LoRaSim simple':>15} {'ours simple':>12} {'diff':>7}   (delivery ratio, mean of 6 seeds)")
    with cf.ProcessPoolExecutor() as pool:
        for n, iv in cases:
            row = {}
            for full in (1, 0):
                ls = list(pool.map(lorasim, [(n, iv * 1000, full, s) for s in range(1, 7)])); us = list(pool.map(ours, [(n, float(iv), full, s) for s in range(1, 7)]))
                row[full] = (statistics.mean(ls), statistics.mean(us), statistics.pstdev(ls), statistics.pstdev(us))
            f, sm = row[1], row[0]
            print(f"{n:>5} {iv:>6} s | {f[0] * 100:12.1f}% {f[1] * 100:10.1f}% {100 * (f[1] - f[0]):+6.1f} | {sm[0] * 100:14.1f}% {sm[1] * 100:11.1f}% {100 * (sm[1] - sm[0]):+6.1f}")


if __name__ == "__main__":
    main()
