# PalavariRaksha — BoatNode FreeRTOS Firmware

**Phase 1: FreeRTOS Sensor Acquisition Foundation**
ESP32-S3 · Arduino Framework · FreeRTOS

---

## Project Context

**PalavariRaksha** (Telugu: "Fishermen's Protection") is a LoRa Mesh Maritime Emergency Safety System for the Chennai coastal fishing community.

This repository contains the **BoatNode embedded firmware** — the Phase 1 FreeRTOS foundation that acquires sensor data concurrently, maintains a unified telemetry struct, and monitors system health. It is designed to be extended in Phase 2 with the six local safety algorithms and in Phase 3 with LoRa/JALARI packet transmission.

---

## Hardware

| Component | Part | Interface | GPIO |
|---|---|---|---|
| MCU | ESP32-S3 | — | — |
| GPS | NEO-6M | UART2 | RX=16, TX=17 |
| Compass | HMC5883L | I2C (0x1E) | SDA=21, SCL=22 |
| Accelerometer | ADXL345 | I2C (0x53) | SDA=21, SCL=22 |
| RTC | DS3231 | I2C (0x68) | SDA=21, SCL=22 |

> ⚠️ **IMPORTANT**: All three I2C devices share the same bus (GPIO21/22).  
> Add 4.7 kΩ pull-up resistors on SDA and SCL lines for reliable 400 kHz operation.

---

## Wiring Diagram

```
ESP32-S3
│
├─ GPIO16 (RX) ─────── NEO-6M TX
├─ GPIO17 (TX) ─────── NEO-6M RX
│
├─ GPIO21 (SDA) ────── HMC5883L SDA ─── ADXL345 SDA ─── DS3231 SDA
├─ GPIO22 (SCL) ────── HMC5883L SCL ─── ADXL345 SCL ─── DS3231 SCL
│
├─ 3.3V ────────────── All sensor VCC
└─ GND  ────────────── All sensor GND

ADXL345 specific:
  SDO pin → VCC   (sets I2C address to 0x53, not 0x1D)
  CS  pin → VCC   (enables I2C mode)
```

---

## Required Libraries

Install via **Arduino Library Manager** (`Tools → Manage Libraries...`):

| Library | Version tested | Arduino Library Manager name |
|---|---|---|
| TinyGPSPlus | ≥ 1.0.3 | `TinyGPSPlus` by mikalhart |
| Adafruit HMC5883 Unified | latest | `Adafruit HMC5883 Unified` |
| Adafruit ADXL345 | latest | `Adafruit ADXL345` |
| Adafruit Unified Sensor | latest | `Adafruit Unified Sensor` |
| RTClib | latest | `RTClib` by Adafruit |

---

## Project Structure

```
BoatNode_FreeRTOS/
│
├── BoatNode_FreeRTOS.ino   ← Main sketch: setup() + task creation
├── config.h                ← All constants (pins, timing, thresholds)
│
├── telemetry.h             ← BoatTelemetry struct + extern sync handles
├── telemetry.cpp           ← Global definitions, mutex/queue creation
│
├── gps_task.h              ← GPS task interface
├── gps_task.cpp            ← NEO-6M reader + TinyGPSPlus parser
│
├── compass_task.h          ← Compass task interface
├── compass_task.cpp        ← HMC5883L reader + heading computation
│
├── imu_task.h              ← IMU task interface
├── imu_task.cpp            ← ADXL345 reader + g-unit conversion
│
├── rtc_task.h              ← RTC task interface
├── rtc_task.cpp            ← DS3231 reader + timestamp formatting
│
├── telemetry_task.h        ← Telemetry print task interface
├── telemetry_task.cpp      ← Serial Monitor debug output
│
├── system_health.h         ← Health task interface
├── system_health.cpp       ← Health queue + heartbeat + state machine
│
└── README.md               ← This file
```

---

## Arduino IDE Setup

1. Install **ESP32 Arduino Core** via Board Manager:
   - URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Board Manager → search "esp32" → Install "esp32 by Espressif Systems"

2. Select board: **ESP32S3 Dev Module**

3. Board settings:
   - CPU Frequency: **240 MHz**
   - Flash Mode: **QIO 80MHz**
   - Partition Scheme: **Default 4MB with spiffs**
   - USB CDC On Boot: **Enabled** (for USB Serial on ESP32-S3)

4. Install all libraries from the table above.

5. Open `BoatNode_FreeRTOS.ino` in Arduino IDE.

6. Select the correct COM port and upload.

---

## FreeRTOS Architecture

```
Core 0                              Core 1
──────────────────────────          ──────────────────────────
GPS Task (Priority 4)               Compass Task (Priority 3)
Telemetry Task (Priority 2)         IMU Task (Priority 3)
System Health Task (Priority 1)     RTC Task (Priority 2)
                │                                │
                └──────── BoatTelemetry ──────────┘
                         (xTelemetryMutex)
                                │
                      xHealthQueue (events)
```

### Task Summary Table

| Task | Priority | Stack | Core | Period | Reason |
|---|---|---|---|---|---|
| GPS Task | 4 | 8 KB | 0 | 1000 ms | Highest: must drain UART buffer |
| Compass Task | 3 | 4 KB | 1 | 100 ms | 10 Hz heading for course tracking |
| IMU Task | 3 | 4 KB | 1 | 50 ms | 20 Hz catches 2–4s impact events |
| RTC Task | 2 | 4 KB | 1 | 1000 ms | Time changes slowly |
| Telemetry Task | 2 | 6 KB | 0 | 1000 ms | Serial output, not time-critical |
| System Health | 1 | 4 KB | 0 | 2000 ms | Background watchdog (lowest) |

### Synchronization Objects

| Object | Type | Purpose |
|---|---|---|
| `xTelemetryMutex` | Mutex | Protects `g_telemetry` struct access |
| `xI2CMutex` | Mutex | Serializes I2C bus access across 3 sensors |
| `xHealthQueue` | Queue (10 items) | Edge-triggered sensor fault/recovery events |

---

## Expected Serial Monitor Output

After boot (115200 baud):

```
========================================
  PalavariRaksha — BoatNode FreeRTOS
  Phase 1: Sensor Acquisition Framework
========================================
  CPU Freq   : 240 MHz
  Free heap  : 287432 bytes
  SDK version: v5.x.x
========================================
[SETUP] I2C bus started: SDA=21 SCL=22 @ 400000 Hz
[SETUP] Telemetry struct initialized
[SETUP] Sync objects created: mutex(telemetry), mutex(I2C), queue(health)
[SETUP] Creating FreeRTOS tasks...
[GPS]      Task started on Core 0. UART2 RX=16 TX=17 @ 9600 baud
[COMPASS]  Task started on Core 1
[IMU]      Task started on Core 1. Period: 50 ms (20 Hz)
[RTC]      Task started on Core 1
[TELEMETRY] Task started on Core 0
[HEALTH]   Task started on Core 0

==============================
     PALAVARIRAKSHA — BOATNODE
==============================
Time : 2026-09-05 18:30:21

[GPS]
  Fix        : YES (8 satellites, HDOP 1.2)
  Latitude   : 13.082700
  Longitude  : 80.270700
  Speed      : 14.20 knots
  Course     : 185.3 deg
  Last update: 42 ms ago

[COMPASS]
  Heading    : 182.5 deg
  Magnetic   : X=0.123 Y=-0.045 Z=0.231 (gauss)

[IMU - ADXL345]
  Accel X    : +0.012 g
  Accel Y    : -0.008 g
  Accel Z    : +0.998 g  (expect ~1.0g at rest)

[RTC - DS3231]
  Valid      : YES

[SYSTEM HEALTH]
  GPS        : OK
  Compass    : OK
  IMU        : OK
  RTC        : OK
  Overall    : HEALTHY

[MEMORY]
  Free heap  : 243120 bytes
  Min heap   : 238944 bytes

[HEARTBEATS (ms since last run)]
  GPS        : 23
  Compass    : 18
  IMU        : 11
  RTC        : 31
==============================
```

---

## Sensor Failure Handling

| Failure | Effect | Recovery |
|---|---|---|
| GPS disconnected | `gpsValid = false`, lat/lon = NaN, health → DEGRADED | Auto when signal returns |
| GPS stale (>5s) | `gpsValid = false`, STALE event queued | Auto when new fix arrives |
| HMC5883L I2C fail | `compassValid = false`, health → DEGRADED | Auto on next read cycle |
| ADXL345 I2C fail | `accelValid = false`, health → DEGRADED | Auto on next read cycle |
| ADXL345 frozen | `accelValid = false`, FAULT event queued | Auto when readings change |
| DS3231 I2C fail | `rtcValid = false`, timestamp → uptime-based fallback | Auto on next read cycle |
| Task heartbeat timeout | health → CRITICAL | Requires reboot (ESP32 TWDT) |
| ≥3 sensors invalid | health → CRITICAL | Requires investigation |

---

## Testing Checklist

- [ ] 1. ESP32 boots and Serial output appears
- [ ] 2. All 6 FreeRTOS tasks start (check Serial log)
- [ ] 3. GPS receives valid fix (lat/lon populated)
- [ ] 4. Compass returns heading (0–360°)
- [ ] 5. ADXL345 returns X/Y/Z values (Z ≈ 1.0g at rest)
- [ ] 6. DS3231 returns current date/time
- [ ] 7. All 3 I2C devices work simultaneously (no I2C errors)
- [ ] 8. Unplug GPS → system continues, gpsValid = false
- [ ] 9. Reconnect GPS → gpsValid recovers automatically
- [ ] 10. Block compass I2C → compassValid = false, GPS continues
- [ ] 11. Block IMU I2C → accelValid = false, GPS/compass continue
- [ ] 12. Remove DS3231 → rtcValid = false, fallback timestamp active
- [ ] 13. Shared telemetry remains consistent (no garbled prints)
- [ ] 14. Free heap stays stable (no leak over 30+ minutes)
- [ ] 15. Stack high-water marks reported in health output
- [ ] 16. DEGRADED state when 1 sensor is pulled
- [ ] 17. HEALTHY state restored when sensor reconnected

---

## How to Tune Task Timing

All periods are defined in `config.h`:

```c
#define PERIOD_GPS_MS        1000   // Increase to reduce CPU usage at cost of latency
#define PERIOD_COMPASS_MS    100    // Decrease to 50ms for faster heading updates
#define PERIOD_IMU_MS        50     // Decrease to 10ms (100 Hz) for Phase 2 impact detection
#define PERIOD_RTC_MS        1000   // Can increase to 5000ms (RTC changes slowly)
#define PERIOD_TELEMETRY_MS  1000   // Increase to 5000ms to reduce Serial spam
#define PERIOD_HEALTH_MS     2000   // Decrease to 500ms for faster fault detection
```

---

## Phase 2 Connectivity Plan

This Phase 1 FreeRTOS layer connects to later phases as follows:

### Six Local Safety Outcomes (Phase 2)
A new `SafetyTask` (Priority 2, Core 0) will:
1. Read `g_telemetry` via `xTelemetryMutex`
2. Run the six outcome algorithms on the unified data
3. Write results back to reserved fields in `BoatTelemetry`

The six outcomes will check: position/speed, IMBL geofencing, GPS noise reduction, dead reckoning, abnormal motion/impact, and multi-sensor health fusion.

### JALARI Packet Creation (Phase 3)
The JALARI packet fields map directly to `BoatTelemetry`:
- `timestamp` → JALARI time field
- `latitude/longitude` → position
- `gpsSpeed/gpsCourse` → motion
- `compassHeading` → heading
- `accelX/Y/Z` → IMU payload
- Safety outcome flags → alert bits

### LoRa/SX1262 Communication (Phase 3)
A new `LoRaTask` (Priority 3) will:
1. Receive JALARI packets from a packet queue
2. Call SX1262 radio driver transmit functions
3. Handle ACK/retry/duplicate detection per the Jalari v3 binary protocol

---

## License

B.Tech Final Year Project — PalavariRaksha  
RESQ-LoRa / Jalari Organization  
For academic use.
