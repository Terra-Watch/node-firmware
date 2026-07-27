# Neighbor

Purpose: maintain a bounded, passive view of nodes heard through valid heartbeat packets.

Public API: `NeighborManager` stores neighbor state; `NeighborService` owns event handling, expiry, and metric updates. `find` and `count` offer read-only access to the fixed sixteen-entry table.

Design: fixed rolling samples calculate average RSSI/SNR. The table also tracks packet success/failure counts, ACK success rate, sequence number, and last-seen time without heap allocation.
