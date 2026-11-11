# TerraWatch relay firmware

SX1278 (433 MHz) only, no sensors. Two builds: `relay_esp32` for the NodeMCU-32S board (classic ESP32, ESP-32S module, the default) and `relay` for an ESP32-S3 devkit. It receives station packets and forwards them toward the gateway (multi-hop demo: station 0x01 -> relay 0x10 -> gateway 0xFE).

It has no code of its own beyond `src/main.cpp`: libraries, config and `App.cpp` come from `../node-firmware`, so a protocol or routing fix there reaches the relay. Node ID is `TW_NODE_ID` in `platformio.ini` (16 = 0x10).

The gateway sends no route advertisements, so the relay keeps a direct route to 0xFE alive itself (refreshed every 20 s, route lifetime is 60 s).

```
pio run                  # builds relay_esp32; set upload_port for your machine
pio run -e relay         # the ESP32-S3 devkit instead
```

Wiring for the NodeMCU-32S (SX1278 module, 3.3 V only; fit the 433 MHz antenna before transmitting):

| SX1278 | NodeMCU-32S pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCK | P18 |
| MISO | P19 |
| MOSI | P23 |
| NSS (CS) | P5 |
| RST | P14 |
| DIO0 | P26 |

P18, P19, P23 and P5 are the board's default VSPI pins. P0, P2, P12 and P15 are boot-strapping pins and P6 to P11 are the flash: keep the radio off them. The classic ESP32 has no built-in USB, so the board's USB-serial chip is used for flashing and the console.

Status: builds. Not flashed, no RF test. Open: the return path (gateway acknowledgement back to the station through the relay) needs the relay to know a route to the station; not checked.
