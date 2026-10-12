#!/usr/bin/env python3
"""Bounded UART monitor for the documented Wokwi VS Code RFC2217 port."""
import argparse
import time
import sys
import serial
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='rfc2217://localhost:4007')
parser.add_argument('--command', default=None)
parser.add_argument('--seconds', type=float, default=2)
args = parser.parse_args()
if args.seconds <= 0:
    parser.error('--seconds must be positive')
with serial.serial_for_url(args.url, baudrate=115200, timeout=0.05) as port:
    if args.command:
        port.write((args.command + '\n').encode('ascii'))
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        data = port.read(4096)
        if data:
            sys.stdout.write(data.decode('ascii', errors='replace'))
            sys.stdout.flush()
