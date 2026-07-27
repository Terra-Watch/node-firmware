# BoatProtocol

Purpose: own packet representation, validation, wire serialization, and parsing.

Public API: `Packet`, `Parser`, `Serializer`, and `Validator`. The header includes a 16-bit per-source sequence number, TTL, and hop count.

Roadmap: introduce versioned packet formats only through this module.

Decision: protocol code has no radio or application dependencies.
