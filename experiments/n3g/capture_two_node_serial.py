#!/usr/bin/env python3
"""Capture two serial streams with host timestamps and source labels.

Requires: python3 -m pip install pyserial
This script does not infer packet results or fabricate measurements; it records
the lines emitted by each board for later analysis.
"""
import argparse
import datetime as dt
import os
import threading
import time

import serial


def capture(label, port, baud, output, stop):
    try:
        device = serial.Serial()
        device.port = port
        device.baudrate = baud
        device.timeout = 0.2
        device.dtr = False
        device.rts = False
        with device, open(output, "a", encoding="utf-8") as log:
            while not stop.is_set():
                raw = device.readline()
                if not raw:
                    continue
                stamp = dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds")
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                record = f"[{stamp}][{label}] {line}\n"
                log.write(record)
                log.flush()
                print(record, end="")
    except serial.SerialException as error:
        print(f"[{label}] serial error: {error}")
        stop.set()


def main():
    parser = argparse.ArgumentParser(description="Capture two Jalari serial logs")
    parser.add_argument("node_a_port")
    parser.add_argument("node_b_port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=0.0, help="seconds; 0 means until Ctrl-C")
    parser.add_argument("--output-dir", default="logs")
    args = parser.parse_args()
    if args.node_a_port == args.node_b_port:
        parser.error("node A and node B ports must be different")
    os.makedirs(args.output_dir, exist_ok=True)
    stop = threading.Event()
    threads = [
        threading.Thread(target=capture, args=("A", args.node_a_port, args.baud, os.path.join(args.output_dir, "node_a.log"), stop), daemon=True),
        threading.Thread(target=capture, args=("B", args.node_b_port, args.baud, os.path.join(args.output_dir, "node_b.log"), stop), daemon=True),
    ]
    for thread in threads:
        thread.start()
    try:
        if args.duration > 0:
            time.sleep(args.duration)
        else:
            while not stop.is_set():
                time.sleep(0.25)
    except KeyboardInterrupt:
        pass
    stop.set()
    for thread in threads:
        thread.join(timeout=1.0)


if __name__ == "__main__":
    main()
