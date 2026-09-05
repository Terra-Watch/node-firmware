# Services

Purpose: keep application scheduling and event coordination out of `App` while retaining deterministic, non-RTOS execution.

Public API: `HeartbeatService` schedules heartbeat packets, `NeighborService` updates and expires neighbor state, `ReliableLinkService` drives hop-by-hop ACK timers, `ForwardingService` applies next-hop forwarding, and `DtnStoreForwardService` provides bounded route-unavailable DATA holding.

Decision: these services contain active policy; their underlying modules remain focused on data storage, packet construction, and reliable-link mechanics.
`DtnStoreForwardService` provides lightweight bounded RAM store-and-forward for
DATA packets that have no currently usable route. It stores the original packet
identity plus local priority and enqueue time, expires entries after the
configured residence time, and releases at most a small batch per update using
fresh `DynamicForwardingStrategy` decisions. SOS, normal, and background
priorities are local metadata; heartbeat packets are not placed in this queue.
The queue evicts expired entries first, then the lowest-priority oldest entry,
and never changes Protocol v4 packets. N3F transmission failure handling remains
separate, and persistent DTN/Bundle Protocol behavior is out of scope.
