# Network Services Review

## Architecture Score

The architecture scores **8.5/10** for the implemented scope. The networking foundation preserves fixed storage, deterministic polling, and existing dependency direction while making mesh-oriented extension points explicit.

## Decisions

| Change | Why it exists |
| --- | --- |
| TTL and hop count in `Packet` | Every packet carries forwarding lifecycle state, independent of any routing algorithm. |
| `ForwardingService` plus `IForwardingStrategy` | TTL enforcement now has one owner; future routing changes the strategy, not packet, radio, or queue code. |
| `NullForwardingStrategy` | Provides explicit current local-only behavior without inventing routing state. |
| Services layer | Removes heartbeat scheduling, neighbor policy, and retry timer driving from `App`. |
| `NeighborManager` as a data store | Keeps metric storage separate from event/timing policy, making it deterministic and testable. |
| ACK outcome events | Supplies link metrics without coupling Neighbor and ReliableLink modules. |

## Remaining Technical Debt

- No PlatformIO unit-test framework is configured yet; the fixed interfaces are test-ready, but host tests should be added before routing work.
- Link metrics are local observations only. They intentionally do not infer route quality or estimate remote loss.
- The fixed neighbor table drops a new neighbor when all sixteen slots are active; eviction policy is deferred until product requirements define its safety tradeoff.
- `NullForwardingStrategy` means packets are never forwarded today. This is intentional; enabling a forwarding strategy before routing requirements would create accidental mesh behavior.
