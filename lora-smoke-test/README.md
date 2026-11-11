# LoRa Basic Smoke Test

Minimal diagnostic firmware for verifying SX1278 LoRa transceiver connectivity and basic RF communication on ESP32-S3 boards.

## Overview

This is a **test utility**, not a production component. It contains two standalone PlatformIO projects — a transmitter and a receiver — that send and receive simple plain-text LoRa packets. The purpose is to verify that the SX1278 hardware is correctly wired, SPI communication works, and basic RF transmission/reception functions before using the full Jalri protocol firmware.

## Role in Jalri

This test is a **supporting diagnostic tool**. It is not part of the Jalri production architecture and does not use the Jalri binary protocol. It exists to validate hardware setup independently of the application firmware.

```text
[ Transmitter Board ] --LoRa 433MHz--> [ Receiver Board ]
     "Hello N"                           Logs HEX + ASCII + RSSI + SNR
```

## Projects

### transmitter

Sends incrementing `"Hello N"` text packets every 2 seconds over LoRa.

- Initializes SX1278 via SPI
- Reads and logs the SX1278 `REG_VERSION` register (expected: `0x12`)
- Sends plain-text LoRa packets
- Logs TX result (success/failure)

### receiver

Listens continuously for LoRa packets and logs diagnostic information.

- Initializes SX1278 via SPI with verbose boot markers
- Reads and logs the SX1278 `REG_VERSION` register at multiple stages
- Receives packets and logs: packet size, RSSI (dBm), SNR (dB)
- Outputs raw HEX bytes and ASCII representation
- Notes if packet length matches expected BoatNode heartbeat (25 bytes) or Gateway ACK (21 bytes) sizes

## Configuration

Both projects use identical SPI pin mapping, matching the Jalri production firmware:

| Pin | GPIO | Function |
|---|---|---|
| SCK | 12 | SPI clock |
| MISO | 13 | SPI data in |
| MOSI | 11 | SPI data out |
| CS/NSS | 10 | Chip select |
| RESET | 9 | Module reset |
| DIO0 | 14 | Interrupt |

**Radio settings:**

| Parameter | Value |
|---|---|
| Frequency | 433 MHz |
| Spreading Factor | 7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Sync Word | 0x12 |
| CRC | Enabled |

## Technology Stack

| Component | Technology |
|---|---|
| MCU | ESP32-S3-DevKitM-1 |
| LoRa library | sandeepmistry/LoRa ^0.8.0 |
| Build system | PlatformIO |
| Framework | Arduino |

## Build and Flash

```bash
# Transmitter
cd transmitter
pio run --target upload
pio device monitor

# Receiver
cd receiver
pio run --target upload
pio device monitor
```

## Expected Output

**Transmitter serial output:**
```
LoRa Transmitter Ready; REG_VERSION=0x12
Settings: 433MHz SF7 BW125 CR4/5 SW0x12 CRC on
Sending diagnostic packet: 0
TX result: success
Sending diagnostic packet: 1
TX result: success
```

**Receiver serial output (when receiving):**
```
RX packetSize=7 bytesRead=7 RSSI=-45 dBm SNR=9.50 dB
NOTE: length does not match expected heartbeat/ACK size
HEX: 48 65 6C 6C 6F 20 30
ASCII:Hello 0
```

## Limitations

- **Not production code** — uses plain-text packets, not the Jalri binary protocol
- **No git repository** — this is a local test utility, not tracked as a separate GitHub repository
- **Two boards required** — transmitter and receiver must run on separate ESP32-S3 boards with SX1278 modules
- **Hardware dependent** — results depend on physical wiring and RF environment
