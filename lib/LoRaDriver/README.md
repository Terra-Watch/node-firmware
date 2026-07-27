# LoRaDriver

Purpose: adapt the Sandeep Mistry LoRa library and SX1278 wiring to `IRadio`.

Public API: `lora::LoRaDriver`.

Roadmap: modem configuration and CAD can be added behind the existing interface.

Decision: SPI setup is delegated to HAL; this module owns only SX1278 library interaction.
