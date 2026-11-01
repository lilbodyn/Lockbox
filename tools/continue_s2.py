#!/usr/bin/env python3
"""Continue an already-running S2 without resetting its measurement window.

Run only after the original S2 collector releases the serial port. Host time
limits the wait; the target is confirmed solely by the actual LB PERF reply.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--version', required=True)
parser.add_argument('--after-ms', type=int, default=3650000)
parser.add_argument('--timeout', type=int, default=1800)
parser.add_argument('--url', default='rfc2217://localhost:4007')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
raw = args.output / 'uart.raw'
start = time.monotonic()
last_stats = -30.0
buffer = bytearray()
version_seen = False
confirmed = False
commands = []
with serial.serial_for_url(args.url, baudrate=115200, timeout=.05) as port, raw.open('wb') as out:
    port.write(b'help\n')
    commands.append('help')
    while time.monotonic() - start < args.timeout:
        data = port.read(4096)
        if data:
            out.write(data)
            out.flush()
            buffer.extend(data)
        while b'\n' in buffer:
            line, _, remaining = buffer.partition(b'\n')
            buffer = bytearray(remaining)
            if b'LB VERSION' in line and args.version.encode() in line:
                version_seen = True
            match = re.search(rb'LB STATUS ms=(\d+)', line)
            if version_seen and match and int(match.group(1)) >= args.after_ms:
                now = time.monotonic() - start
                if now - last_stats >= 15:
                    port.write(b'stats\n')
                    commands.append('stats')
                    last_stats = now
            match = re.search(rb'LB PERF window_us=(\d+)', line)
            if version_seen and match and int(match.group(1)) >= 3600000000:
                confirmed = True
        if confirmed:
            port.write(b'persist\n')
            commands.append('persist')
            end = time.monotonic() + 2
            while time.monotonic() < end:
                data = port.read(4096)
                if data:
                    out.write(data)
            break
result = {'version_seen': version_seen, 'mcu_window_confirmed': confirmed,
          'host_elapsed_s': time.monotonic() - start, 'commands': commands,
          'uart_sha256': hashlib.sha256(raw.read_bytes()).hexdigest(),
          'note': 'UART window alone does not prove clock accuracy; check long VCD.'}
(args.output / 'run.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
raise SystemExit(0 if confirmed else 1)
