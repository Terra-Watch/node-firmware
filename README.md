# TerraWatch field node firmware

ESP32-S3 + SX1278 (433 MHz) LoRa firmware for the fixed forest stations. See `../docs/13-lookout-stations.md` for what the stations are for and `../docs/14-field-firmware-plan.md` for the build plan.

Roles: sensing station (PMS5003 + BME680, this folder), relay (`../relay-firmware`, LoRa only, no sensors), gateway (`../gateway-firmware`).

## What is here
- Binary protocol v4: 11-byte header, payload up to 48 bytes, CRC-16 (`lib/Protocol`). No authentication yet (planned: truncated HMAC).
- Heartbeats, neighbour table with RSSI/SNR window, ack and retry (3 tries), TTL and hop count.
- Multi-hop: static routes, dynamic route learning and advertisement, failure recovery, store-and-forward (`lib/Routing`, `lib/Forwarding`, `lib/Services`).
- Telemetry (`lib/Telemetry`): BME680 temperature and humidity every 15 s as a 15-byte DATA packet to the gateway (PM fields are 0xFFFF until the PMS5003 is added).
- Sound module (`lib/Acoustic`, `lib/Sensors`): MAX4466 energy trigger plus INMP441 capture, logged only; switch off with `-D TW_ACOUSTIC=0`. No classifier yet.
- USB serial is on the board's native USB port (build flags `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`).

## Build and test
```
pio run                                   # build for the S3 dev board
bash test/run_protocol_v4_tests.sh        # host tests: one script per test in test/
```
Known: `run_n1_tests.sh` and `run_n2c_failure_recovery_tests.sh` already failed before the TerraWatch clean-up (checked 2026-11-02); not yet investigated.

## Layout
`src/` composition root and app loop; `lib/` one folder per module (HAL, LoRaDriver, Protocol, PacketFactory, PacketQueue, Neighbor, Routing, Forwarding, ReliableLink, Services, EventBus, Utils); `include/` config and constants; `test/` host tests. Design notes: `ARCHITECTURE.md`, `ARCHITECTURE_REVIEW.md`, `NETWORK_SERVICES_REVIEW.md`.
