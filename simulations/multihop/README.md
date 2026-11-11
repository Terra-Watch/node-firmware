# Multi-hop delivery simulation

`mesh_sim.py` simulates many lookout stations reporting to one gateway over a LoRa collection tree: path loss with shadowing and fading, capture,
half-duplex radios, the lowest-ETX parent, the hop-by-hop ACK with retries, and the firmware's 8-packet queue. It compares four configurations:

| config | what it adds |
|---|---|
| baseline | what the firmware did: no listen-before-talk, no random delay, route adverts every 15 s, no end-to-end receipt, no backup parent |
| +lbt | random delay before the first attempt, listen-before-talk with exponential back-off |
| +e2e | the origin keeps each **alert** until the gateway's receipt arrives, resends after 12/20/30 s |
| full | + backup parent (instant failover), adverts every 60 s |
| full-300 | the same with adverts every 300 s (the route lifetime would have to grow to match) |

`python3 mesh_sim.py` runs the full table (6 seeds, 2 h simulated, about 3 min); `python3 mesh_sim.py quick` is a small run. Results go to `results.json`.
Alerts (sound events, about one per station per 30 min) are the only traffic the end-to-end receipt covers; telemetry is replaced by the next reading.
Resending telemetry as well was tried and lost packets at a 60 s interval: the extra load congests the single gateway.

## Results (packets that reached the gateway, mean of 6 seeds)

Reports every 10 min, no failure:

| stations | baseline | +lbt | full | full-300 |
|---|---|---|---|---|
| 40 | 71.8 % | 99.4 % | 98.1 % | 99.0 % |
| 80 | 41.4 % | 98.7 % | 98.3 % | 99.0 % |

Alerts at 80 stations: 39.9 % baseline, 98.9 % +lbt, 99.4 % +e2e, 99.2 % full, 99.9 % full-300.

One relay (the one carrying the most stations) dies half way, reports every 10 min, 80 stations: baseline 22.3 %, +lbt 98.3 %, full 99.3 %.
Hop retries per packet fall from 19.0 (baseline) to 0.98 (full), and the channel is busy 20 % of the time instead of 53 %.

Reports every minute is too much for one gateway: with 40 stations only 66 % arrive even with listen-before-talk (baseline 5 %).
Past roughly 20 stations at that rate the answer is a second gateway, not a better algorithm.

## What the simulation does not show

- The radio parameters (path loss exponent, foliage loss, shadowing) are plausible for 433 MHz under canopy, not measured. Measure real link ranges before quoting a station count.
- Positions are a jittered grid at 1.4 km spacing with the gateway in a corner.
- Forwarding is modelled as unicast to the chosen parent, which is what protocol v5 does: the next hop is a byte in the header (byte 11), and a node that is not named only overhears. A packet with no usable route is sent with the next hop 0xFF and any node with a route may carry it.
- Most of the remaining airtime is route adverts (60-90 % of it). Slower adverts (full-300) show the room that is left.
