#!/usr/bin/env python3
"""Prepare an explicitly erased NVM image for a Wokwi cold-start experiment.

The STM32 simulation starts omitted flash addresses as zero in the observed run.
This fixture initializes both reserved pages to their hardware-erased value. It
does not implement or emulate a flash erase/program peripheral. Never flash this
image when preserving an existing hardware journal is required.
"""
import argparse
from pathlib import Path

def record(address,kind,data):
    values=bytes([len(data),address>>8,address&255,kind])+bytes(data)
    return ':'+(values+bytes([-sum(values)&255])).hex().upper()

def factory(source,destination):
    lines=source.read_text().splitlines(); base=0; result=[]; extent=0
    for line in lines:
        raw=bytes.fromhex(line[1:]); assert sum(raw)%256==0
        n,address,kind=raw[0],int.from_bytes(raw[1:3],'big'),raw[3]
        assert len(raw)==n+5
        if kind==0:
            assert base+address+n<=0x08007000, 'source overlaps journal'
            extent=max(extent,base+address+n)
        elif kind==4: base=int.from_bytes(raw[4:6],'big')<<16
        elif kind==2: base=int.from_bytes(raw[4:6],'big')<<4
        if kind!=1: result.append(line)
    assert extent>0x08000000
    result.append(record(0,4,[8,0]))
    for offset in range(0x7000,0x8000,16): result.append(record(offset,0,[255]*16))
    result.append(record(0,1,[]))
    destination.write_text('\n'.join(result)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('destination',type=Path)
    a=p.parse_args();factory(a.source,a.destination)
