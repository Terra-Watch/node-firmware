# Utils

Purpose: hold small platform-neutral utilities such as CRC and logging policy.

Public API: `Crc16`, `Logger`, and buffer helpers.

Roadmap: keep only shared utilities with no domain ownership here.

Decision: Logger depends on HAL's console abstraction, never directly on Arduino Serial.
