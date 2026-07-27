# Architecture Review

## Score

The pre-refactor architecture scored **4/10**. Packet serialization and radio communication were correctly separated, but application code still owned hidden dependencies and platform details leaked through utilities. The resulting design would have become difficult to test or safely extend.

The current architecture scores **8/10** for its present scope. It has explicit ownership, one-way dependencies, fixed memory infrastructure, and host-test seams. The remaining score reflects intentional omissions: no host test suite yet, no board abstraction beyond required hardware services, and no future feature modules before their requirements exist.

## Weaknesses Corrected

| Previous weakness | Why it limited growth | Architecture now |
| --- | --- | --- |
| `App` created a hidden static `LoRaDriver` | Tests and alternate radios could not be substituted | `App` receives `IRadio` through its constructor |
| `NodeManager` was a singleton | State leaked between tests and obscured lifetime | The composition root owns an injected node object |
| Arduino calls existed in radio, logger, time, and main | Hardware dependencies spread through the codebase | ESP32 clock, delay, SPI, and console adapters live in HAL |
| Packet construction lived in `App` | Packet semantics would be duplicated as features grew | `PacketFactory` owns approved packet creation |
| Transmission was immediate and unbuffered | Future MAC scheduling would require invasive rewrites | A bounded ring-buffer `PacketQueue` is the transmit boundary |
| Modules called each other for lifecycle outcomes | New consumers would increase coupling | `EventBus` publishes fixed, synchronous events |
| Constants were scattered across modules | Runtime configuration and board variants would be error-prone | Immutable `FirmwareConfig` groups node, radio, protocol, and power configuration |
| Logger depended directly on `Serial` | Logging could not be used on a host or alternate console | Logger uses HAL `IConsole` and supports compile-time severity filtering |

## Dependency Rules

1. Only `HAL` includes Arduino headers or invokes Arduino APIs.
2. `App` depends on domain modules and interfaces, never an ESP32 driver or LoRa library.
3. `LoRaDriver` is the SX1278 adapter and is the only component that includes the third-party LoRa header.
4. `main.cpp` is the sole composition root; process-lifetime static storage is confined there because Arduino requires global `setup` and `loop` entry points.
5. Shared infrastructure has bounded, allocation-free storage. RTOS ownership and asynchronous dispatch are deliberately deferred.

## Intentional Non-Changes

The existing heartbeat transmission remains unchanged in behavior. Its movement into `PacketFactory` and `PacketQueue` is architectural migration only. No ACK, routing, CAD, TDMA, RTOS, sensor, GPS, DTN, BLE, gateway, OTA, or security behavior was added.
