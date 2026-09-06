# Jalari BoatNode Firmware

Embedded firmware for the Jalri BoatNode — the vessel-side component of the Jalri maritime safety system. The BoatNode transmits periodic heartbeats and supports reliable data communication over LoRa using a custom binary protocol.

## Overview

The BoatNode firmware runs on an ESP32-S3 microcontroller paired with an SX1278 LoRa transceiver (433 MHz). It implements a modular, interface-driven architecture designed around dependency injection, fixed-memory infrastructure, and deterministic polling.

The firmware handles:

- Periodic heartbeat transmission to the Jalri Gateway
- Reliable unicast data communication with acknowledgement tracking and retry
- Neighbor discovery and link-quality metric collection
- Packet forwarding infrastructure (currently configured for local-only delivery)

## Role in Jalri

```text
[ BoatNode ] --LoRa--> [ Gateway ] --HTTP--> [ Backend ] --> [ Dashboard ]
     ▲                                                            
  This repo                                                       
```

The BoatNode is the origin of all vessel-side communication. It transmits heartbeat and data packets over LoRa, which are received, validated, and acknowledged by the Jalri Gateway.

## Implemented Features

- **Heartbeat transmission** — periodic heartbeat packets at a configurable interval (default: 2000 ms)
- **Custom binary protocol (v4)** — 11-byte header with preamble, version, packet type, source/destination IDs, previous-hop ID, sequence number, TTL, hop count, and payload size, followed by payload and CRC-16
- **Static multi-hop forwarding (N2B)** — bounded local destination-to-next-hop routes; `nextHopId` is never added to the wire packet
- **Reliable unicast** — AckManager tracks up to 8 pending DATA packets, handles acknowledgement matching, and re-enqueues timed-out packets up to 3 retries
- **Neighbor management** — fixed-capacity table (16 entries) with rolling link metrics (RSSI/SNR window of 8 samples) and configurable neighbor timeout (default: 30 s)
- **Forwarding infrastructure** — ForwardingService enforces TTL/hop-count rules and uses bounded static next-hop selection through IForwardingStrategy
- **EventBus** — synchronous, fixed-capacity (8 listeners) event system for decoupled module communication
- **PacketQueue** — bounded ring buffer (8 slots) serving as the transmit boundary
- **PacketFactory** — centralized packet construction with protocol-enforced defaults
- **Hardware Abstraction Layer (HAL)** — all Arduino/ESP32 APIs accessed through interfaces (IClock, IDelay, IConsole, ISpiBus)
- **Logger** — severity-filtered logging through HAL IConsole, no direct Serial dependency
- **CRC-16** — CCITT CRC-16 for packet integrity

## Architecture

```text
main.cpp (composition root)
  │
  ├── App
  │     ├── IRadio (LoRaDriver)
  │     ├── PacketQueue → BoatProtocol → IRadio
  │     └── EventBus (synchronous, fixed-capacity)
  │
  ├── Services
  │     ├── HeartbeatService
  │     ├── HeartbeatAckManager
  │     ├── NeighborService → NeighborManager
  │     ├── ReliableLinkService → AckManager
  │     └── ForwardingService → IForwardingStrategy (NullForwardingStrategy)
  │
  ├── BoatProtocol
  │     ├── Packet / PacketFactory
  │     ├── Parser / Serializer
  │     └── Validator
  │
  ├── HAL (ESP32 adapters)
  │     ├── Esp32Clock, Esp32Delay
  │     ├── Esp32SerialConsole
  │     └── Esp32SpiBus
  │
  └── Utils (Logger, Crc16, BufferUtils, TimeUtils)
```

Dependencies point inward. Only HAL and LoRaDriver include Arduino or third-party headers. App depends on interfaces and domain modules only.

## Technology Stack

| Component | Technology |
|---|---|
| MCU | ESP32-S3-DevKitM-1 |
| LoRa transceiver | SX1278 (via sandeepmistry/LoRa library) |
| Build system | PlatformIO |
| Framework | Arduino |
| Language | C++17 (`-std=gnu++17`) |
| Flash | 16 MB |
| PSRAM | Enabled |

## Project Structure

```text
jalari-node-firmware/
├── include/
│   ├── Config.h          # FirmwareConfig: radio, node, protocol, power, reliability, network
│   ├── Constants.h       # Compile-time sizes and capacities
│   └── Version.h         # Firmware name and version (0.1.0)
├── lib/
│   ├── BoatProtocol/     # Packet, Parser, Serializer, Validator
│   ├── EventBus/         # Synchronous event bus
│   ├── Forwarding/       # IForwardingStrategy, NullForwardingStrategy
│   ├── HAL/              # ESP32 hardware adapters + interfaces
│   ├── IRadio/           # Radio interface
│   ├── LoRaDriver/       # SX1278 radio adapter
│   ├── Neighbor/         # NeighborManager (fixed-size table + link metrics)
│   ├── Node/             # NodeManager
│   ├── PacketFactory/    # Approved packet construction
│   ├── PacketQueue/      # Bounded ring-buffer transmit queue
│   ├── ReliableLink/     # AckManager (reliable unicast)
│   ├── Services/         # HeartbeatService, HeartbeatAckManager, NeighborService,
│   │                     # ReliableLinkService, ForwardingService
│   └── Utils/            # Logger, Crc16, BufferUtils, TimeUtils
├── q/                    # Pre-refactor snapshot (older App/main code)
├── test/                 # Placeholder (no host tests configured)
├── platformio.ini        # Build configuration
├── ARCHITECTURE.md       # Architecture description
├── ARCHITECTURE_REVIEW.md # Architecture review and scoring
└── NETWORK_SERVICES_REVIEW.md # Network services review
```

## Configuration

All configuration is defined in [`Config.h`](include/Config.h) as an immutable `FirmwareConfig` struct:

| Group | Key Parameters | Defaults |
|---|---|---|
| Radio | Frequency, SPI pins (SCK=12, MISO=13, MOSI=11, CS=10, RST=9, DIO0=14) | 433 MHz |
| Node | Node ID | 1 |
| Protocol | Preamble, version, max payload, max radio packet | 0xA5, v4, 48 bytes, 61-byte serialized packet |
| Power | Heartbeat interval | 2000 ms |
| Reliability | ACK timeout, max retries | 1000 ms, 3 retries |
| Network | Default TTL, max hop count, neighbor timeout, metric window | 8, 16, 30 s, 8 samples |

## Build

```bash
# Build (default environment: esp32-s3-devkitm-1)
pio run

# Upload to connected board
pio run --target upload

# Serial monitor
pio device monitor
```

Requires [PlatformIO](https://platformio.org/) CLI or IDE extension.

## Testing

### N1 reliability foundation

The direct DATA link uses `(sourceId, sequence)` as its current identity. Successful
transmission waits for a direct type-3 ACK; immediate radio failures and ACK timeouts
use the configured bounded retry count (three retries after the initial attempt).
Duplicates are ACKed again for unicast DATA but delivered only once. Duplicate state is
bounded and expires after 10 seconds. Broadcast DATA (`0xFF`) is locally deliverable
without a unicast ACK. These behaviors are host-tested; RF behavior is not validated.

- **Host tests**: `test/run_n1_tests.sh` covers protocol, ACK, retry, immediate TX failure, duplicate, queue, destination, broadcast, invalid packet, and TTL behavior.
- **Protocol contract test**: A cross-repository host test exists in [`jalri-gateway-firmware/test/`](../jalri-gateway-firmware/test/protocol_contract_test.cpp) that validates BoatNode packet serialization against Gateway parsing.
- **Hardware validation**: Not performed. Physical RF communication between BoatNode and Gateway has not been tested.

### N2C — Failure and Recovery Validation

Deterministic host/simulation tests validate the static A → B → C → Gateway path under
baseline traffic, ACK loss, bounded retry exhaustion, duplicate retransmission, missing
routes, route restoration for new packets, TTL/hop limits, queue overflow, loops, wrong
destinations, and broadcast non-rebroadcast behavior. Automatic alternate paths and
MAC/channel access remain unimplemented. Dynamic forwarding, route failure invalidation,
and bounded lightweight DTN store-and-forward are implemented. Physical RF validation
remains pending.

### N3B — Dynamic Routing Metric Foundation

N3B adds a bounded 16-observation rolling window for ACK outcomes. The metric is explicitly
ACK success probability, not PDR: `successful ACKs / total ACK observations`. At least two
observations are required for a valid metric; stale observations after the configured
neighbor lifetime become unavailable. The future ETX-style cost is `1 / max(p_ack, 0.1)`,
bounded at 10.0. RSSI and SNR remain diagnostic measurements and do not affect routing.
Dynamic routes, advertisements, and next-hop selection remain future N3C/N3D/N3E work.

### N3C — Bounded Dynamic Route Table

N3C adds a fixed 16-entry `RouteTable` with one primary unicast route per destination.
Entries store destination, next hop, fixed-point route cost, hop count, timestamp, version,
and validity. The table consumes N3B `LinkMetric` values but does not discover, advertise,
select, invalidate through link failures, or forward routes. Stale entries are unusable and
can be explicitly expired; Protocol v4 and the existing static forwarding path are unchanged.

### N3D — Dynamic Neighbor Discovery and Route Learning

N3D reuses heartbeat as the bounded neighbor-liveness beacon. A compact v4 route-advertisement
packet carries destination, advertised route metric, hop count, and route version.
`RouteAdvertisementService` combines advertised route cost with the local N3B link metric,
updates `RouteTable`, suppresses duplicate advertisements, and rate-limits emission per
destination. Dynamic forwarding now consumes fresh primary routes from RouteTable; static
forwarding remains available as a regression baseline. N3F invalidates failed primary
routes, and lightweight DTN holds route-unavailable DATA in bounded RAM. Full DTN and
Bundle Protocol semantics are not implemented.

## Current Status

| Category | Status |
|---|---|
| Architecture | Implemented (scored 8.5/10 in self-review) |
| Heartbeat TX | Implemented in code |
| Reliable unicast | Implemented in code |
| Neighbor management | Implemented in code |
| Forwarding infrastructure | Implemented (bounded StaticForwardingStrategy) |
| Host test suite | N1, Protocol v4, N2B, and N2C scripts |
| Hardware validation | Not performed |
| Firmware version | 0.1.0 |

## Integration

- **Gateway**: The BoatNode transmits protocol v4 packets that the [Gateway firmware](../jalri-gateway-firmware/) receives, parses, validates, and acknowledges. `PreviousHop` identifies the immediate transmitter; forwarding remains a later N2 milestone.
- **Protocol contract**: Verified through a host-compilable contract test that serializes a BoatNode heartbeat and parses it with the Gateway's parser/validator.

## Intentional Non-Changes

The following are explicitly **not implemented** and are deferred by design:

- Dynamic routing and alternate-path selection
- RTOS / asynchronous dispatch
- GPS, sensors, BLE
- OTA updates
- Security / encryption
- TDMA / CAD / channel access
- Full DTN / Bundle Protocol semantics
- Neighbor eviction policy
