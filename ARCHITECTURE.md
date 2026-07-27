# Jalari BoatNode Architecture

`main.cpp` is the composition root. It creates concrete ESP32 HAL adapters, configuration, and the application graph exactly once. It is the only place that selects ESP32 implementations.

Application flow is `App -> IRadio`, `App -> PacketQueue -> BoatProtocol -> IRadio`; parsed frames are published as `EventBus` events. The current bus is synchronous and fixed-capacity, which keeps RAM use and execution ordering deterministic before an RTOS is introduced.

Reliable unicast DATA handling is event-driven: `AckManager` observes received packets, queues acknowledgements, tracks up to eight sent DATA packets, and re-enqueues each timed-out packet at most three times. `NeighborService` observes heartbeats and maintains `NeighborManager`'s fixed-size table. Neither module accesses the radio directly.

Network services are deterministic polling components invoked by `App`: `HeartbeatService` schedules heartbeats, `NeighborService` maintains rolling link metrics and expires stale entries, and `ReliableLinkService` drives retransmission timeouts. `ForwardingService` applies packet TTL and hop-count rules before using `IForwardingStrategy`; the installed `NullForwardingStrategy` always delivers locally. Future routing replaces only that strategy.

The protocol version is 3 because TTL and hop count extend every packet header. Packets created by `PacketFactory` start with the configured default TTL and zero hops. A forwarding decision decrements TTL before enqueueing and increments hop count; TTL expiry emits `TtlExpired` followed by `PacketDropped`.

Dependencies point inward: App depends on interfaces and domain modules; `LoRaDriver` adapts the third-party radio library; HAL owns Arduino API access. Node identity, radio wiring, protocol limits, and power timing are immutable members of `FirmwareConfig`.

The existing heartbeat behavior is retained solely as current firmware behavior. PacketFactory and PacketQueue make no new packet type or scheduling policy available.
