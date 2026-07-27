# IRadio

Purpose: define the radio boundary used by application code.

Public API: `IRadio` and `ReceivedFrame`.

Roadmap: alternative LoRa chip drivers and host test fakes can implement this interface.

Decision: it represents the common packet-radio contract, not SX1278 register operations.
