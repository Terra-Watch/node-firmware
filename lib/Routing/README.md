# Routing

N3C provides the bounded `RouteTable` and `RouteEntry` state layer. It stores one primary
route per unicast destination, accepts already-computed N3B link/route metrics, applies
version and quality ordering, and expires stale entries. It does not advertise, discover,
select, or forward routes; those responsibilities belong to later N3 milestones.

Route metrics use fixed-point cost units: `cost * 100`. The table capacity is 16 entries.

N3D adds `RouteAdvertisementService`, which reuses heartbeat-based neighbor state and learns
routes from compact route advertisements. It does not activate dynamic forwarding or implement
route discovery, recovery, DTN, or Bundle Protocol.
Its bounded `update()` path periodically advertises fresh learned routes through the normal
packet queue, subject to the configured per-destination interval.

N3F adds `RouteFailureService`. It consumes only definitive
`AcknowledgementFailed` events after retry exhaustion, invalidates the matching
destination/next-hop entry, and leaves subsequent selection to
`DynamicForwardingStrategy`. The table has one primary route per destination, so
N3F does not provide alternate candidates or multipath routing. A later N3D route
advertisement can restore a route for new packets. Failed packets are not stored;
bounded store-and-forward remains a future DTN milestone.
