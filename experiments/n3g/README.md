# N3G two-node RF validation

This plan covers the two physical boards currently attached to the laptop. The
laptop is used only for flashing and timestamped serial capture; LoRa traffic
must remain over RF.

## Current inspected configuration

- BoatNode target: `esp32-s3-devkitm-1`, 433 MHz, SF7, BW 125 kHz, CR 4/5,
  sync word `0x12`, hardware CRC enabled.
- Serial baud: 115200.
- Default BoatNode ID: `0x01`; build-time override is `JALARI_NODE_ID`.
- Current visible macOS ports at inspection time were
  `/dev/cu.usbmodem5B5E0265961` and `/dev/cu.usbmodem5B5E0268161`.
  Port-to-board mapping must be confirmed locally; it is not inferred here.
- The current project has no classic ESP32 PlatformIO environment. The ESP32
  board therefore needs a confirmed compatible board target before flashing.

Build distinct identities after selecting the correct board target:

```text
build_flags = -D JALARI_NODE_ID=1   # Node A
build_flags = -D JALARI_NODE_ID=2   # Node B
```

Do not flash the same default-ID image to both boards.

## Serial capture

Install the host dependency and capture both streams:

```bash
python3 -m pip install pyserial
python3 experiments/n3g/capture_two_node_serial.py \
  /dev/cu.usbmodem5B5E0265961 /dev/cu.usbmodem5B5E0268161 \
  --duration 120 --output-dir experiments/n3g/logs/trial-01
```

The firmware now emits timestamp-free packet diagnostics such as
`[RF TX]` and `[RF RX]`; the capture tool adds UTC host timestamps and source
labels. Board clock and host timestamp are not claimed to be synchronized.

## Development DATA injection

The serial command interface is compiled only when `JALARI_TEST_MODE=1`.
Commands are bounded and use the normal `PacketFactory` and transmission queue:

```text
SEND_DATA 2 HELLO
SEND_DATA_COUNT 2 100 TEST
```

The repeated command enqueues one packet per firmware update, so it does not
create a tight radio loop. Each packet receives its sequence number from the
existing factory. Use the host helper to send a command:

```bash
python3 experiments/n3g/send_command.py /dev/cu.usbmodemXXXX "SEND_DATA 2 HELLO"
```

Build the two test images with distinct IDs and `JALARI_TEST_MODE=1`. The
current repository has only an ESP32-S3 environment; do not claim the classic
ESP32 is supported until its board target and pin mapping are confirmed.

Expected diagnostics include `[RF TX]`, `[RF RX]`, `ACK_RX`, `TX_RETRY`, and
`TX_FAILED`. The command payload contains no experiment metadata. Test mode is
disabled by default and no packets are sent automatically at boot.

Populate `results_template.csv` only from captured observations. No distances,
PDR, RSSI, SNR, ACK, retry, or latency values are prefilled.

## Experiments

For each trial record the exact board IDs, ports, antenna arrangement, measured
distance, start/end time, packet count, and failure injections.

1. **A → B basic link** — run one transmitter and one receiver, verify RF RX
   lines, source/destination, sequence, payload size, RSSI/SNR, and CRC-valid
   parser acceptance. This proves a physical one-hop link only.
2. **B → A basic link** — reverse the roles and compare asymmetric results.
3. **ACK/reliability** — use a controlled unicast DATA source if available;
   compare ACK events, timeout, retries, and exhaustion. The command interface
   is available only in the explicitly selected `JALARI_TEST_MODE=1` builds.
4. **PDR** — send a known count, preferably 100 packets per direction and five
   trials when practical; count RF RX records by `(source, sequence)`.
5. **Latency** — compute TX-to-RX and, when correlated, TX-to-ACK latency from
   host capture timestamps. Treat host timestamp resolution and USB buffering
   as measurement limitations.
6. **RSSI/SNR** — extract values from RX diagnostics for each received frame.
7. **Distance** — repeat the PDR/latency/RSSI/SNR procedure only at physically
   measured, reproducible distances.
8. **Node disappearance/recovery** — stop or reset B, record A's last success,
   retry exhaustion, then restart B and record the first subsequent success.
   This validates one-link interruption/recovery, not multi-hop N3F recovery.
9. **Duplicates/invalid frames** — only if a safe approved injector exists;
   do not corrupt production packets or alter the protocol to manufacture them.
10. **Bidirectional traffic** — run controlled near-simultaneous traffic if the
    application can generate it. Measure current behavior only; no MAC claim.

## Capability matrix

| Feature | Two-node RF | Host/software | More hardware required |
|---|---:|---:|---:|
| LoRa TX/RX | Yes | Yes | No |
| Protocol v4 fields/CRC | Yes, for observed frames | Yes | No |
| DATA ACK/retry | Conditional on DATA injection | Yes | No |
| PDR, latency, RSSI, SNR | Yes | Yes | No |
| Distance comparison | Yes, if physically measured | No | No |
| Duplicate/invalid packets | Conditional on safe injector | Yes | No |
| Node disappearance/link recovery | Yes | Yes | No |
| Dynamic forwarding N3E | No | Yes | Third forwarding node |
| N3F multi-hop recovery | No | Yes | At least three nodes |
| DTN partial behavior | Conditional; no current DATA injector | Yes | No for software |
| Three-hop mesh | No | Yes | At least four endpoints/nodes as applicable |
| Gateway mesh | No with only these BoatNodes | Yes | Physical Gateway or approved gateway board |

## MAC status

The current LoRa driver exposes `cad()` but returns `false`; no LBT,
randomized backoff, or TDMA implementation was found. These experiments measure
the current radio behavior and do not validate a future MAC design.

## Evidence boundary

No RF experiment was run by this session. Host tests, serial capture, and
PlatformIO builds are not RF validation. N3E/N3F multi-hop behavior remains
software-validated until additional physical forwarding nodes are available.
