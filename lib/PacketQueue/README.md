# PacketQueue

Purpose: provide deterministic FIFO storage for packets awaiting transmission.

Public API: enqueue, dequeue, peek, empty, full, size, and clear.

Roadmap: a future MAC scheduler may consume this queue. ACK generation and retransmission already enqueue through this single boundary.

Decision: fixed storage avoids heap allocation and is intentionally not an RTOS queue.
