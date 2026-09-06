# Neighbor

Purpose: maintain a bounded, passive view of nodes heard through valid heartbeat packets.

Public API: `NeighborManager` stores neighbor state, bounded RSSI/SNR samples, and bounded
ACK observations; `NeighborService` owns event handling, expiry, and metric updates. `find`,
`count`, and `getLinkMetric` expose neighbor state without introducing route selection.

Design: fixed rolling samples calculate average RSSI/SNR. The table also tracks packet success/failure counts, ACK success rate, sequence number, and last-seen time without heap allocation.
