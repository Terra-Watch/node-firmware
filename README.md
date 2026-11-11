# TerraWatch field node firmware

ESP32-S3 + SX1278 (433 MHz) LoRa firmware for the fixed forest stations. See `../docs/13-lookout-stations.md` for what the stations are for and `../docs/14-field-firmware-plan.md` for the build plan.

Roles: sensing station (this folder), relay (`relay/`, LoRa only, no sensors, same libraries), gateway (`../gateway-firmware`).

```
station --LoRa--> [relay ...] --LoRa--> gateway --serial--> (bridge, later) --> backend
```

## What a station does
- **Air quality and weather** (`lib/Telemetry`, `lib/Sensors`): BME680 temperature and humidity every 15 s as a 15-byte DATA packet to the gateway (tag `S`). PM2.5 and PM10 from a PMS5003 are filled in when the sensor is present; otherwise those fields are 0xFFFF.
- **Position**: the station has no GPS. Its fixed position is set at install (`TW_STATION_LAT_E6`, `TW_STATION_LON_E6`, micro-degrees) and sent to the gateway at start and every 10 minutes (tag `P`).
- **Sound** (`lib/Acoustic`, `lib/Sensors`): a MAX4466 energy trigger starts an INMP441 capture. An int8 classifier (`mn10_as`, TFLite Micro with ESP-NN, from `../acoustic-ml`) labels 1 s of 16 kHz audio as chainsaw, blast, fire, wood, wind or background. One classification takes about 1.85 s on the S3. Alert logic (`AlertLogic`) raises chainsaw (at least 2 of the last 3 windows within 60 s), blast (one window) and fire (2 of the last 3 windows within 60 s at a high threshold, at most one alert per 10 minutes); wood and wind are logged, not alerted. An alert is a 4-byte DATA packet (tag `A`, kind, confidence in percent). Thresholds are starting points, not calibrated in the field. The model needs the `node-acoustic` build (PSRAM, 6 MB app partition, model embedded from `model/`); other builds run the trigger only (`-D TW_ACOUSTIC=0` switches sound off).
- **Radio link**: binary protocol v5 (`lib/Protocol`): 12-byte header (the next hop is in the header), payload up to 48 bytes, CRC-16. No authentication yet (planned: truncated HMAC). Heartbeats, neighbour table with RSSI/SNR window, hop-by-hop ack with 3 tries, TTL and hop count.
- **Multi-hop** (`lib/Routing`, `lib/Forwarding`, `lib/Services`): static and learned routes, route advertisements, failure recovery, store-and-forward, listen-before-talk with a random delay, a backup parent that takes over on retry, and an end-to-end receipt from the gateway for alerts (the origin resends an alert until the receipt arrives). Telemetry is not resent; the next reading replaces it. `../simulations/multihop` compares these steps on a simulated network.
- **Status LED** (`TW_LED_PIN`): blue air quality sent, red chainsaw, magenta blast, orange fire, white position.
- USB serial is on the board's native USB port; the `node-acoustic` build uses the UART USB port at 921600 baud (slower logging corrupts radio reception on these boards).

## Build and test
```
pio run                                # S3 dev board, no sound model
pio run -e node-acoustic               # ESP32-S3 N16R8 with the sound classifier (set the ports in platformio.ini)
bash test/run_protocol_tests.sh     # host tests: one script per test in test/ (14 scripts)
```
Host tests (run 2026-11-05): 12 of 14 scripts pass. `run_reliability_tests.sh` and `run_failure_recovery_tests.sh` abort and have not been investigated; they failed before the TerraWatch clean-up as well.

## Layout
`src/` composition root and app loop; `lib/` one folder per module (Acoustic, Sensors, Telemetry, HAL, LoRaDriver, Protocol, PacketFactory, PacketQueue, Neighbor, Routing, Forwarding, ReliableLink, Services, EventBus, Utils); `include/` config and constants; `model/` the int8 sound model; `test/` host tests. Design notes: `ARCHITECTURE.md`, `ARCHITECTURE_REVIEW.md`, `NETWORK_SERVICES_REVIEW.md`.
