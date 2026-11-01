#!/usr/bin/env python3
"""S3 UART capture with GO: events must be proven separately from VCD."""
import argparse
import hashlib
import json
import re
import time
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',required=True,type=Path)
parser.add_argument('--go',required=True,type=Path)
args=parser.parse_args()
if args.go.exists(): raise SystemExit('GO must not exist before start')
args.output.mkdir(parents=True,exist_ok=False)
import serial
origin=time.monotonic(); last_rx=origin; last_status=origin
phase='GATE'; reason='interrupted'; version=False; help_ack=False; status_ack=False
first=None; last=None; max_records=0; queue=[]; index=0; buffer=bytearray(); commands=[]
raw=args.output/'uart.raw'; status=args.output/'state.json'
def save(**kw):
    d={'phase':phase,'reason':reason,'version':version,'help_ack':help_ack,'status_ack':status_ack,
       'last_mcu_ms':last,'max_records_observed':max_records,'host_elapsed_s':time.monotonic()-origin,**kw}
    t=status.with_suffix('.tmp');t.write_text(json.dumps(d,indent=2)+'\n');t.replace(status)
try:
    with serial.serial_for_url('rfc2217://localhost:4007',baudrate=115200,timeout=.02) as port, raw.open('wb') as out, (args.output/'timing.jsonl').open('w') as log:
        def send(cmd):
            port.write((cmd+'\n').encode()); entry={'kind':'tx','host_monotonic_ns':time.monotonic_ns(),'command':cmd}
            log.write(json.dumps(entry)+'\n');log.flush();commands.append(entry)
        send('help');send('status');save()
        while True:
            now=time.monotonic();data=port.read(4096)
            if data:
                last_rx=now;out.write(data);out.flush();buffer.extend(data)
                log.write(json.dumps({'kind':'rx','host_monotonic_ns':time.monotonic_ns(),'bytes':len(data)})+'\n');log.flush()
            if now-last_rx>15 or now-last_status>15: reason='UART/STATUS stalled';break
            if phase=='GATE' and now-origin>120: reason='15 MCU-second gate timeout';break
            while b'\n' in buffer:
                line,_,tail=buffer.partition(b'\n');buffer=bytearray(tail);line=bytes(line).strip()
                if line.startswith(b'LB BOOT') and first is not None: raise RuntimeError('unexpected reset')
                if line.startswith(b'LB VERSION'):
                    if b'484fae964f8e7cb1' not in line: raise RuntimeError('wrong source')
                    version=True
                m=re.match(rb'LB COMMAND id=(\d+)',line)
                if m and phase=='GATE':
                    help_ack |= int(m.group(1))==7;status_ack |= int(m.group(1))==1
                m=re.match(rb'LB STATUS ms=(\d+).* records=(\d+)',line)
                if m:
                    ms=int(m.group(1));max_records=max(max_records,int(m.group(2)));last_status=now
                    if last is not None and ms<last: raise RuntimeError('MCU timestamp regression')
                    last=ms
                    if phase=='GATE' and version:
                        if first is None: first=(ms,now)
                        if ms-first[0]>=15000 and help_ack and status_ack:
                            dm=(ms-first[0])/1000;dh=now-first[1]
                            if abs(dm-dh)>max(1.0,dh*.1): raise RuntimeError('short gate mismatch')
                            phase='WAIT_ROOT_GO';save(gate_mcu_s=dm,gate_host_s=dh)
            if phase=='WAIT_ROOT_GO' and args.go.exists():
                if args.go.read_text().strip()!='S3 GO 484fae964f8e7cb1': raise RuntimeError('wrong GO')
                start=now; phase='RUN'; queue=[(0,'sample 20')]
                for n in range(10): queue.extend([(1+n*1.2,'arm'),(1.75+n*1.2,'disarm')])
                queue.extend([(14,'status'),(15,'stats reset')])
                queue.extend((16+n*.4,'journal') for n in range(115))
                queue.extend((16+n*2.0,'status') for n in range(23))
                queue += [(63,'stats'),(64,'imu 69'),(68,'status'),(69,'imu 68'),(74,'status')]
                for n,p in enumerate((10,5,2,1)):
                    queue.extend([(76+n*5,f'sample {p}'),(79+n*5,'stats')])
                queue.extend([(97,'sample 20'),(98,'persist'),(99,'stats')]);queue.sort();save()
            if phase=='RUN':
                elapsed=now-start
                if elapsed>=16 and max_records<10: raise RuntimeError('journal seed not confirmed; no S3 claim')
                while index<len(queue) and elapsed>=queue[index][0]: send(queue[index][1]);index+=1
                if elapsed>=103: phase='FINISHED';reason='capture complete; VCD and physical event proof required';break
            if now-origin>1200: reason='host safety timeout';break
except (Exception,KeyboardInterrupt) as exc:
    if reason=='interrupted': reason=f'{type(exc).__name__}: {exc}'
finally:
    save(uart_sha256=hashlib.sha256(raw.read_bytes()).hexdigest() if raw.exists() else None,
         command_count=len(commands),note='No S3 pass without >=30 actual overlapping GPIO events in VCD.')
raise SystemExit(0 if phase=='FINISHED' else 1)
