# ReliableLink

Purpose: provide bounded ACK tracking, retransmission, and duplicate detection for unicast DATA packets.

Public API: `AckManager` subscribes to packet events and uses `PacketQueue` for every acknowledgement and retransmission. `DuplicatePacketCache` exposes deterministic `(source ID, sequence number)` detection.

Design: up to eight DATA packets are tracked without heap allocation. Each is retried at the configured timeout at most three times. Duplicate DATA packets are still acknowledged so a sender can stop retrying. ACK success/failure events let `NeighborService` update link metrics without a direct dependency.

Non-goals: routing, CAD, TDMA, RTOS queues, and payload delivery policy remain outside this module.
