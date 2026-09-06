#!/usr/bin/env python3
"""Send one development command to a BoatNode serial port."""
import argparse
import serial

parser = argparse.ArgumentParser()
parser.add_argument("port")
parser.add_argument("command")
parser.add_argument("--baud", type=int, default=115200)
args = parser.parse_args()
device = serial.Serial()
device.port = args.port
device.baudrate = args.baud
device.timeout = 1
device.dtr = False
device.rts = False
with device:
    device.write((args.command + "\n").encode("utf-8"))
