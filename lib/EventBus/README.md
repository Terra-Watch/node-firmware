# EventBus

Purpose: publish lifecycle, packet, neighbor, TTL, heartbeat, and link events to subscribed modules.

Public API: `EventBus`, `Event`, and `IEventListener`.

Roadmap: sensor, GPS, and battery event types can be added when those modules exist.

Decision: fixed listener storage and synchronous dispatch are deterministic; queued/asynchronous dispatch belongs to a future RTOS milestone. ACK outcome events are retained because metrics need them now; no generic message bus was added.
