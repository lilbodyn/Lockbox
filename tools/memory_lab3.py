#!/usr/bin/env python3
"""Summarize retained input sections in GNU ld map, with visible exclusions."""
import json
import re
from collections import defaultdict
from pathlib import Path
root=Path(__file__).resolve().parents[1]
mapfile=root/'firmware/build/lockbox.map'
text=mapfile.read_text().split('Linker script and memory map',1)[1]
modules=defaultdict(lambda:{'flash_bytes':0,'ram_bytes':0})
items=[]; section=None
for line in text.splitlines():
    m=re.match(r'^\s+(\.\S+)',line)
    if m: section=m[1]
    m=re.search(r'\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+\.(?:o|a)(?:\([^)]*\))?)\s*$',line)
    if not m: continue
    address,size=int(m[1],16),int(m[2],16); obj=m[3]
    if not size or not (0x08000000<=address<0x08007000 or 0x20000000<=address<0x20003000): continue
    name='libc/libgcc' if '.a(' in obj else Path(obj).name
    ram=0x20000000<=address<0x20003000
    flash=not ram or bool(section and section.startswith(('.data','.RamFunc')))
    modules[name]['flash_bytes']+=size if flash else 0
    modules[name]['ram_bytes']+=size if ram else 0
    items.append({'section':section,'address':hex(address),'size':size,'object':obj})
frames=[]
for p in (root/'firmware/build').glob('*.su'):
    for line in p.read_text().splitlines():
        x=line.split('\t')
        if len(x)>2: frames.append({'function':x[0],'frame_bytes':int(x[1]),'kind':x[2]})
out={'source_manifest':json.loads((root/'docs/metrics/source-manifest.json').read_text())['source_sha256'],
     'module_retained_input_sections':dict(sorted(modules.items())),
     'excluded':'linker fill, heap/stack reservations, debug sections, discarded input sections; totals can differ by alignment',
     'largest_compiler_frames':sorted(frames,key=lambda x:x['frame_bytes'],reverse=True)[:30],
     'static_stack_limit':'Per-function compiler frames only. Prebuilt newlib/libgcc do not provide .su; do not claim a complete call-chain bound.',
     'input_sections':items}
(root/'docs/metrics/memory-by-module.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out['module_retained_input_sections'],indent=2))
