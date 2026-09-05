# N3G Two-Node Hardware Report

Both connected boards were identified as ESP32-S3 QFN56 revision v0.2 with 8 MB embedded PSRAM. Node A used `/dev/cu.usbmodem5B5E0265961`; Node B used `/dev/cu.usbmodem5B5E0268161`.

Both test images built and flashed successfully. Captured RF logs show bidirectional heartbeat reception and successful one-packet DATA/ACK smoke tests.

Isolated 100-packet baselines recorded 100/100 DATA receptions in each direction. A→B had 85 matching ACK receptions and B→A had 84. RSSI ranged from −63 to −58 dBm; SNR ranged from 9.00 to 10.50 dB. Host-capture TX-to-DATA-RX latency was approximately 1.88 ms mean in both directions; this is USB/host timestamp latency, not synchronized radio latency.

The required five-trial batch was attempted but excluded from results because separate serial capture openings reset the boards and the initial command schedule overlapped active counts. The host capture tools were corrected to avoid DTR/RTS reset, but no replacement five-trial batch was claimed.

The controlled failure attempt was excluded: Node B continued replying with ACKs, so Node B was not actually unavailable. One-hop failure/recovery remains unvalidated.

No distance measurement was performed. No multi-hop, mesh, route-advertisement, DTN, or hardware N3F validation was performed.
