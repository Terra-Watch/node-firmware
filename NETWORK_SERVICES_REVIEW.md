# Network Services Review

## Architecture Score

The architecture scores **8.5/10** for the implemented scope. The networking foundation preserves fixed storage, deterministic polling, and existing dependency direction while making mesh-oriented extension points explicit.

## Decisions

| Change | Why it exists |
| --- | --- |
| TTL and hop count in `Packet` | Every packet carries forwarding lifecycle state, independent of any routing algorithm. |
| `ForwardingService` plus `IForwardingStrategy` | TTL enforcement now has one owner; future routing changes the strategy, not packet, radio, or queue code. |
| `StaticForwardingStrategy` | Provides bounded destination-to-next-hop selection without dynamic routing. |
| Services layer | Removes heartbeat scheduling, neighbor policy, and retry timer driving from `App`. |
| `NeighborManager` as a data store | Keeps metric storage separate from event/timing policy, making it deterministic and testable. |
| ACK outcome events | Supplies link metrics without coupling Neighbor and ReliableLink modules. |

## N2C — Failure and Recovery Validation

The failure suite uses fake clocks, bounded queues, direct service calls, and deterministic
fault injection. It validates ACK loss and retransmission at each hop, duplicate suppression,
missing routes, relay/link failure, route restoration for new packets, TTL/hop boundaries,
queue overflow, static loops, wrong destinations, and broadcast behavior.

Route failure emits `ForwardingRouteNotFound`; link failure reaches bounded retry exhaustion
and emits `TransmissionFailed` plus `AcknowledgementFailed`. Failure is observable through
EventBus at the node, but no end-to-end failure protocol exists for the original application.
Automatic route recovery, alternate paths, dynamic routing, store-and-forward, DTN, and MAC
remain unimplemented. Physical RF validation remains pending.

## N3B — Dynamic Routing Metric Foundation

Neighbor ACK outcomes now use a fixed 16-sample rolling window. One observation represents
one completed reliable-link outcome: an ACK is one success, while retry exhaustion is one
failure. The exposed metric reports `Unknown`, `Valid`, or `Stale`, with validity requiring
two observations. The primary value is ACK success probability, not full bidirectional PDR.
The future ETX-style cost is `1 / max(p_ack, 0.1)`, so zero-success links remain finite at
cost 10.0. RSSI/SNR continue to support diagnostics only. No route table, advertisements,
dynamic selection, or forwarding behavior was added.

## N3C — Bounded Dynamic Route Table

`RouteTable` is a fixed 16-entry state layer between future route learning and future
dynamic forwarding. Each destination has one primary route. Route costs are stored as
fixed-point values (`cost * 100`); valid N3B `LinkMetric` values can be converted at the
API boundary. Version ordering uses serial-number comparison, equal-version candidates use
metric/hop-count/next-hop tie-breaking, and stale entries are immediately unusable.
N3C adds no advertisements, route discovery, dynamic forwarding, or recovery behavior.

## N3D — Dynamic Neighbor Discovery and Route Learning

N3D reuses heartbeat for neighbor liveness and local RSSI/SNR measurement. Route advertisements
use existing v4 framing with a six-byte control payload: destination, advertised fixed-point
route metric, hop count, and route version. The learning service requires a valid local N3B
metric, adds local link cost to advertised route cost, suppresses bounded duplicates, and
rate-limits emission per destination. Static forwarding remains active.

## Remaining Technical Debt

## N1 reliable direct-link foundation

The current direct DATA identity is `(sourceId, sequenceNumber)`. A successful physical
transmission waits for a type-3 direct ACK; immediate radio TX failures and ACK timeouts
schedule at most the configured three retries after the initial attempt. Retry count is
the number of re-transmission attempts, not total attempts. Duplicate DATA is ACKed again
for unicast traffic but is delivered once, using bounded cache entries that expire after
10 seconds. Broadcast DATA (`0xFF`) is locally deliverable without a unicast ACK and is
not rebroadcast. These behaviors are host-tested; RF behavior is not validated.

- Host tests are script-driven rather than PlatformIO unit tests; they cover N1, Protocol v4, N2B, and N2C behavior.
- Link metrics are local observations only. They intentionally do not infer route quality or estimate remote loss.
- The fixed neighbor table drops a new neighbor when all sixteen slots are active; eviction policy is deferred until product requirements define its safety tradeoff.
- Static forwarding is intentionally limited to configured next hops; dynamic route discovery and alternate-path selection are deferred.
