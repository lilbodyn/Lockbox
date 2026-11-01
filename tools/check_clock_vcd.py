#!/usr/bin/env python3
"""Compare MCU uptime with independently timed UART in an actual Wokwi VCD.

Use a quiet capture: status lines preceded by >=20 ms wire idle only. This
avoids claiming UART queueing time as oscillator drift. A busy trace can be
inconclusive. The tolerance is an acceptance condition, not physical calibration.
"""
import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from capture import load_vcd, decode_uart


def inspect(path, start, end, limit, minimum):
    _, _, signals = load_vcd(path)
    decoded = decode_uart(signals.get('UART_TX', []))
    rows, line = [], []
    previous_end = None
    for tick, byte in decoded:
        line.append((tick, byte))
        if byte != 10:
            continue
        text = bytes(v for _, v in line).decode('ascii', errors='replace').strip()
        first, last = line[0][0], line[-1][0]
        idle = None if previous_end is None else (first-previous_end)/1000
        m = re.match(r'LB STATUS ms=(\d+)\b', text)
        if m and start <= first/1e9 <= end and idle is not None and idle >= 20000:
            ms = int(m[1])
            rows.append({'vcd_start_ns': first, 'mcu_ms': ms,
                         'offset_ms': first/1e6-ms, 'wire_idle_us': idle})
        previous_end = last+10*1e9/115200
        line = []
    offsets = [r['offset_ms'] for r in rows]
    span = max(offsets)-min(offsets) if offsets else None
    base = statistics.median(offsets[:5]) if offsets else None
    result = 'inconclusive' if len(rows) < minimum else 'pass' if span <= limit else 'fail'
    return {'vcd': str(path.resolve()), 'vcd_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'interval_s': [start, end], 'eligible_status_lines': len(rows),
            'required_status_lines': minimum, 'offset_span_ms': span,
            'allowed_offset_span_ms': limit, 'baseline_offset_ms': base,
            'result': result, 'rows': rows,
            'scope': 'Digital-model VCD only. Requires quiet UART and no MCU reset/ms wrap within this interval. '
                     'No inference of physical HSI accuracy, clock polling bound outside the capture, or MCU-hour completion.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('vcd', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--start', type=float, default=2)
    p.add_argument('--end', type=float, default=120)
    p.add_argument('--limit-ms', type=float, default=5)
    p.add_argument('--minimum', type=int, default=30)
    a = p.parse_args()
    if a.end <= a.start or a.minimum < 2 or a.limit_ms <= 0:
        p.error('positive tolerance, minimum>=2 and end>start required')
    r = inspect(a.vcd, a.start, a.end, a.limit_ms, a.minimum)
    a.output.write_text(json.dumps(r, indent=2)+'\n')
    print(json.dumps({k: v for k, v in r.items() if k != 'rows'}, indent=2))
    raise SystemExit(0 if r['result'] == 'pass' else 1)


if __name__ == '__main__':
    main()
