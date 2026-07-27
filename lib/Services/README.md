# Services

Purpose: keep application scheduling and event coordination out of `App` while retaining deterministic, non-RTOS execution.

Public API: `HeartbeatService` schedules heartbeat packets, `NeighborService` updates and expires neighbor state, `ReliableLinkService` drives ACK timers, and `ForwardingService` applies TTL and the forwarding extension point.

Decision: these services contain active policy; their underlying modules remain focused on data storage, packet construction, and reliable-link mechanics.
