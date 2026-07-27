# Forwarding

Purpose: define the narrow extension point future routing algorithms use to choose local delivery, forwarding, or drop.

Public API: `IForwardingStrategy`, `ForwardDecision`, and `NullForwardingStrategy`.

Decision: the null strategy only delivers locally. It introduces no route table, discovery, or forwarding traffic; those are deferred until a routing milestone.
