#!/usr/bin/env python3
"""Identify the uncommitted firmware by content, without changing Git history."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
files=sorted([p for p in (root/'firmware').rglob('*') if p.is_file() and
              'build' not in p.relative_to(root).parts and p.name!='build_version.h' and
              p.suffix in {'.c','.h','.s','.ld','.ioc'}]+[root/'firmware/Makefile',root/'diagram.json'])
records={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
digest=hashlib.sha256(json.dumps(records,sort_keys=True).encode()).hexdigest()
base=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
text=f'#define LB_BASE_REV "{base[:12]}"\n#define LB_SOURCE_REV "{digest[:16]}"\n'
p=root/'firmware/Board/build_version.h'
if not p.exists() or p.read_text()!=text: p.write_text(text)
p=root/'docs/metrics/source-manifest.json'; p.parent.mkdir(exist_ok=True)
p.write_text(json.dumps({'base':base,'source_sha256':digest,'files':records},indent=2)+'\n')
print(f'Lockbox base={base} source={digest}')
