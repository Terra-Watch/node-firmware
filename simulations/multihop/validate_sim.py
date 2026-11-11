"""Checks of the multi-hop simulator against theory, the radio chip's formula and published models. Run: python3 validate_sim.py  (about 10 minutes)
  1. airtime against the Semtech formula and published values
  2. a single-hop star against pure ALOHA theory (Abramson; Bor et al. 2016 for LoRa)
  3. sensitivity of the headline result to the radio assumptions that are not measured (foliage loss, shadowing, capture threshold, transmit power, spacing)
Results are printed as tables; the same numbers are in docs/24-hardware-validation.md."""
import concurrent.futures as cf, math, statistics, sys
import mesh_sim as M

# ---- 1. airtime --------------------------------------------------------------------------------------------------------------------------------------
def semtech_airtime(pl, sf=7, bw=125e3, cr=1, preamble=8, crc=True, header=True, de=False):
    """AN1200.13 (SX127x datasheet, section 4.1.1.6)."""
    tsym = 2 ** sf / bw
    tpre = (preamble + 4.25) * tsym
    h = 0 if header else 1
    npay = 8 + max(math.ceil((8 * pl - 4 * sf + 28 + 16 * (1 if crc else 0) - 20 * h) / (4 * (sf - 2 * (1 if de else 0)))) * (cr + 4), 0)
    return tpre + npay * tsym

def check_airtime():
    print("1. Airtime (SF7, 125 kHz, CR 4/5, explicit header, CRC, preamble 8)")
    print("   bytes   simulator   Semtech formula   published")
    published = {10: 0.0412, 51: 0.1027}     # the values every LoRa airtime calculator gives (TTN, Semtech): 41.2 ms and 102.7 ms
    ok = True
    for nb in (10, 16, 18, 20, 24, 29, 51, 62):
        a, b = M.airtime(nb), semtech_airtime(nb)
        pub = published.get(nb)
        print(f"   {nb:>5}   {a * 1000:7.2f} ms   {b * 1000:7.2f} ms      {'' if pub is None else f'{pub * 1000:.1f} ms'}")
        ok &= abs(a - b) < 1e-9 and (pub is None or abs(b - pub) < 0.0006)
    print("   RESULT:", "matches" if ok else "MISMATCH"); return ok

# ---- 2. ALOHA star ----------------------------------------------------------------------------------------------------------------------------------------
def star_place(self):
    self.pos = [(0.0, 0.0)] + [(0.3 * math.cos(2 * math.pi * i / self.n), 0.3 * math.sin(2 * math.pi * i / self.n)) for i in range(self.n)]
    N = self.n + 1; self.rx = [[None] * N for _ in range(N)]
    for a in range(N):
        for b in range(a + 1, N):
            d = max(0.05, math.dist(self.pos[a], self.pos[b]))
            self.rx[a][b] = self.rx[b][a] = M.PTX - (32.44 + 20 * math.log10(433) + 20 * math.log10(d) + M.EXTRA_LOSS + 15 * math.log10(max(d, 0.1) / 0.1))
    self.hear = [[b for b in range(N) if b != a and self.rx[a][b] >= M.CAD_THRESHOLD] for a in range(N)]
    self.nbr = [[b for b in range(N) if b != a and self.rx[a][b] >= M.SENS + 3] for a in range(N)]

def star_one(args):
    n, interval, seed = args
    M.SHADOW_SIGMA, M.FADE_SIGMA, M.HOP_RETRIES = 0.0, 1e-9, 0           # equal powers: no capture, so the channel is pure ALOHA; first try is the only try
    M.Sim.place = star_place
    cfg = dict(lbt=False, e2e=False, backup=False, advert=1e12)
    s = M.Sim(n, interval, cfg, seed, 3000.0, False)
    s.at = s.at     # keep
    r = s.run()
    return r["pdr"]

def aloha_theory(n, interval, t_data, t_ack):
    """First-try delivery in pure ALOHA with a gateway that is deaf while it sends acknowledgements: P = exp(-2 G) * exp(-lambda P (t_data + t_ack)), G = lambda t_data."""
    lam = n / interval; g = lam * t_data; p = math.exp(-2 * g)
    for _ in range(100): p = math.exp(-2 * g) * math.exp(-lam * p * (t_data + t_ack))
    return p, g

def check_aloha():
    print("2. Single-hop star, equal powers, one try (pure ALOHA), against theory")
    print("   stations  interval   G (data)   theory PDR   simulated PDR (6 seeds)   difference")
    td, ta = M.AIR["data"], M.AIR["ack"]; worst = 0.0
    with cf.ProcessPoolExecutor() as pool:
        for n, interval in ((20, 60), (40, 60), (80, 60), (40, 20), (80, 20), (80, 10)):
            runs = list(pool.map(star_one, [(n, interval, s) for s in range(1, 7)]))
            th, g = aloha_theory(n, interval, td, ta); sim = statistics.mean(runs); worst = max(worst, abs(th - sim))
            print(f"   {n:>8}  {interval:>6} s   {g:8.3f}   {th * 100:9.1f} %   {sim * 100:9.1f} % (sd {statistics.pstdev(runs) * 100:.1f})   {100 * (sim - th):+6.1f} points")
    print(f"   RESULT: largest difference {100 * worst:.1f} points"); return worst

# ---- 3. sensitivity ----------------------------------------------------------------------------------------------------------------------------------------
DEFAULTS = {k: getattr(M, k) for k in ("EXTRA_LOSS", "SHADOW_SIGMA", "CAPTURE_DB", "PTX", "FADE_SIGMA", "HOP_RETRIES")}

def sens_one(args):
    name, over, cfgname, n, interval, seed = args
    for k, v in {**DEFAULTS, **over}.items(): setattr(M, k, v)          # reset first: worker processes are reused
    M.AIR = {k: M.airtime(v) for k, v in M.SIZES.items()}
    try: return (name, cfgname), M.Sim(n, interval, M.CONFIGS[cfgname], seed, 3600.0, False).run()
    except RuntimeError: return (name, cfgname), None          # the stations are too far apart to form a connected network under this assumption

def check_sensitivity():
    print("3. Sensitivity of the result (80 stations, one report per 10 min, share reaching the gateway; 3 seeds; baseline = firmware before, full = firmware now)")
    cases = [("default", {}), ("foliage loss 10 dB", {"EXTRA_LOSS": 10.0}), ("foliage loss 30 dB", {"EXTRA_LOSS": 30.0}), ("shadowing 8 dB", {"SHADOW_SIGMA": 8.0}), ("capture 2 dB", {"CAPTURE_DB": 2.0}),
             ("capture 10 dB", {"CAPTURE_DB": 10.0}), ("17 dBm (library default)", {"PTX": 17.0}), ("10 dBm", {"PTX": 10.0})]
    jobs = [(nm, ov, cfg, 80, 600, s) for nm, ov in cases for cfg in ("baseline", "+lbt", "full") for s in (1, 2, 3)]
    out = {}
    with cf.ProcessPoolExecutor(max_workers=None) as pool:
        for key, r in pool.map(sens_one, jobs, chunksize=2): out.setdefault(key, []).append(r)
    out = {k: [r for r in v if r is not None] for k, v in out.items()}
    print(f"   {'assumption':<26} {'baseline':>9} {'+lbt':>8} {'full':>8}   alerts full")
    for nm, _ in cases:
        if not all(out[(nm, c)] for c in ("baseline", "+lbt", "full")): print(f"   {nm:<26} no connected network at 1.4 km spacing (stations out of range of each other)"); continue
        row = [statistics.mean(r["pdr"] for r in out[(nm, c)]) for c in ("baseline", "+lbt", "full")]
        al = statistics.mean(r["pdr_alert"] for r in out[(nm, "full")] if r["pdr_alert"] is not None)
        print(f"   {nm:<26} {row[0] * 100:8.1f}% {row[1] * 100:7.1f}% {row[2] * 100:7.1f}%   {al * 100:6.1f}%")
    return out

if __name__ == "__main__":
    part = sys.argv[1] if len(sys.argv) > 1 else "all"
    if part in ("all", "airtime"): check_airtime()
    if part in ("all", "aloha"): check_aloha()
    if part in ("all", "sens"): check_sensitivity()
