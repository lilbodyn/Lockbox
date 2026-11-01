#!/usr/bin/env python3
"""Record real Lockbox UART; this script never substitutes host policy for MCU.

Requires pyserial and a running, explicitly authorized Wokwi/physical session.
It does not start a simulator. Host monotonic time schedules commands only;
latency analysis must use MCU timestamps or the independently recorded VCD.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import time

ROOT=Path(__file__).resolve().parents[1]

def plan(scenario,duration,rate):
    events=[(0,'help'),(.4,'sample 20'),(.8,'disarm'),(1.2,'stats reset')]
    if scenario=='S1': pass
    elif scenario=='S2':
        events.append((2,'arm'))
        events += [(t,'status') for t in range(60,int(duration),60)]
    elif scenario=='S3':
        events.append((1.6,'arm\x00suffix')) # Must be rejected as a damaged line.
        # Fill the actual MCU RAM journal before overlapping its export.
        events += [(2+i*.15,'arm' if i%2==0 else 'disarm') for i in range(18)]
        events += [(5,'arm'),(6,'journal')]
        events += [(t,'stats') for t in range(15,int(duration),15)]
    elif scenario=='fault':
        events += [(2,'arm'),(4,'imu 69'),(8,'status'),(12,'imu 68'),(17,'status')]
    elif scenario=='boundary':
        for i,period in enumerate((20,10,5,2,1)):
            t=2+i*35
            events.extend([(t,f'sample {period}'),(t+.4,'stats reset'),(t+30,'stats')])
        events.append((178,'sample 20'))
    events.extend([(duration,'stats'),(duration+.5,'persist')])
    return {'scenario':scenario,'duration_s':duration,'requested_tx_rate_Bps':rate,
            'events':sorted(events),'manual_stimulus':
            'S3: >=30 physical MODE presses, plus separate PIR/SHOCK/LIGHT; capture BUTTON/REACTION/VCD. '
            'No command is a substitute for an actual GPIO event.'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('scenario',choices=['S1','S2','S3','fault','boundary'])
    p.add_argument('--url',default='rfc2217://localhost:4007')
    p.add_argument('--seconds',type=float)
    p.add_argument('--rate',type=int,default=1280,help='S3 requested host bytes/s, not measured wire throughput')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--firmware',type=Path,default=ROOT/'firmware/build/lockbox.hex')
    p.add_argument('--diagram',type=Path,default=ROOT/'diagram.json')
    p.add_argument('--plan-only',action='store_true')
    a=p.parse_args()
    duration=a.seconds or {'S1':60,'S2':3660,'S3':90,'fault':20,'boundary':185}[a.scenario]
    if duration<=0 or a.rate<0: p.error('duration must be positive and rate nonnegative')
    run=plan(a.scenario,duration,a.rate)
    run['source_manifest']=json.loads((ROOT/'docs/metrics/source-manifest.json').read_text())
    run['elf_sha256']=hashlib.sha256((ROOT/'firmware/build/lockbox.elf').read_bytes()).hexdigest()
    run['url']=a.url
    run['firmware_file']=str(a.firmware.resolve())
    run['firmware_sha256']=hashlib.sha256(a.firmware.read_bytes()).hexdigest()
    run['diagram_file']=str(a.diagram.resolve())
    run['diagram_sha256']=hashlib.sha256(a.diagram.read_bytes()).hexdigest()
    a.output.mkdir(parents=True,exist_ok=False)
    (a.output/'plan.json').write_text(json.dumps(run,ensure_ascii=False,indent=2)+'\n')
    if a.plan_only:
        print('Plan only; no UART connection or MCU execution.'); return
    import serial
    log=a.output/'uart.raw'
    commands=[]; received=bytearray(); index=0; flood_next=6.0; written=0
    expected=run['source_manifest']['source_sha256'][:16]
    # pyserial 3.5 RFC2217 does not support write_timeout.
    with serial.serial_for_url(a.url,baudrate=115200,timeout=.01) as port, log.open('wb') as out:
        start=time.monotonic()
        while True:
            elapsed=time.monotonic()-start
            data=port.read(4096)
            if data: out.write(data); out.flush(); received.extend(data)
            if elapsed>2 and expected.encode() not in received:
                raise RuntimeError('Expected MCU version not seen; keep raw log and inspect wrong/stale firmware')
            while index<len(run['events']) and elapsed>=run['events'][index][0]:
                scheduled,command=run['events'][index]; payload=(command+'\n').encode('ascii')
                port.write(payload); written+=len(payload)
                commands.append({'host_elapsed_s':elapsed,'scheduled_s':scheduled,'command':command}); index+=1
            if a.scenario=='S3' and 6<=elapsed<duration-1 and a.rate and elapsed>=flood_next:
                payload=b'status\njournal\nprobe\n'
                port.write(payload); written+=len(payload); flood_next=elapsed+len(payload)/a.rate
            if elapsed>=duration+3: break
        run['host_elapsed_s']=time.monotonic()-start
    run['written_bytes']=written; run['commands']=commands
    run['uart_sha256']=hashlib.sha256(log.read_bytes()).hexdigest()
    run['mcu_version_confirmed']=expected.encode() in received
    windows=[int(x) for x in re.findall(rb'LB PERF window_us=(\d+)',received)]
    run['last_measured_window_us']=windows[-1] if windows else None
    if a.scenario=='S2':
        run['hour_confirmed_on_mcu']=bool(windows and windows[-1]>=3600000000)
    (a.output/'run.json').write_text(json.dumps(run,ensure_ascii=False,indent=2)+'\n')
    print(f'Saved {len(received)} actual UART bytes. No physical-event pass is inferred.')

if __name__=='__main__': main()
