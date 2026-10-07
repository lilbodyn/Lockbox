#!/usr/bin/env python3
"""Fail when portable sources acquire a dependency on board or vendor code."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
portable = [root / 'firmware' / name for name in ('Guard', 'Sensors')]
failures = []
for directory in portable:
    for source in sorted(directory.glob('*.[ch]')):
        text = source.read_text()
        for header in re.findall(r'#\s*include\s*[<"]([^>"\n]+)', text):
            allowed = header in {'stdbool.h','stdint.h','stddef.h','string.h'} or any((d/header).is_file() for d in portable)
            if not allowed:
                failures.append(f'{source.relative_to(root)}: forbidden include {header}')
        if re.search(r'\b(HAL_\w+|GPIO[A-Z]|I2C1|USART2|ADC1)\b', text):
            failures.append(f'{source.relative_to(root)}: hardware symbol')
if failures:
    raise SystemExit('\n'.join(failures))
print('PASS portable include graph and hardware independence')
