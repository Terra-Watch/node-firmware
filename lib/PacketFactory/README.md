# PacketFactory

Purpose: create valid semantic protocol packets without transmitting them.

Public API: `PacketFactory::createHeartbeat`, `createData`, and `createAcknowledgement`.

Roadmap: add factories for already-approved protocol packet types.

Decision: application code never manually fills packet wire-model fields; packet source identity and sequence allocation are centralized here.
