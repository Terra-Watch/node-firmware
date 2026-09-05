# Forwarding

Purpose: define the narrow extension point future routing algorithms use to choose local delivery, forwarding, or drop.

Public API: `IForwardingStrategy`, `ForwardDecision`, `NullForwardingStrategy`, the bounded `StaticForwardingStrategy`, and `DynamicForwardingStrategy`.

`StaticForwardingStrategy` selects a local next hop from `NetworkConfig::staticRoutes`. The next hop is local metadata and is never serialized; the protocol source and final destination remain unchanged. Missing routes return `ForwardDecision::RouteNotFound`.

`DynamicForwardingStrategy` reads the fresh primary route for the packet's final
destination from `RouteTable`. It selects only a valid unicast next hop; it does
not maintain routing state, broadcast data, or recover failed routes. The existing
`ForwardingService` remains responsible for duplicate handling, TTL/hop updates,
queueing, and the hop-by-hop reliable-link path. Static forwarding remains a
separate regression baseline; DTN/store-and-forward is a later integration point.

The null strategy only delivers locally. It introduces no route table, discovery,
or forwarding traffic.
