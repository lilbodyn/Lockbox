#!/usr/bin/env python3
"""Measure the actual Lockbox eight-channel VCD, preserving its source hash."""
import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from capture import load_vcd, decode_uart

def summary(xs):
    if not xs: return {'n':0}
    ys=sorted(xs)
    return {'n':len(xs),'min_us':min(xs),'avg_us':statistics.mean(xs),
            'max_us':max(xs),'p99_us':ys[min(len(ys)-1,int(.99*len(ys)))]}

def pulses(edges):
    start=None
    for t,v in edges:
        if v: start=t
        elif start is not None:
            yield start,t
            start=None

def analyze(path,out):
    _,_,signals=load_vcd(path)
    decoded=decode_uart(signals.get('UART_TX',[]))
    text=bytes(v for _,v in decoded).decode('ascii',errors='replace')
    out.with_suffix('.uart.txt').write_text(text)
    starts=[a for a,b in pulses(signals['SAMPLE'])]
    widths=[(b-a)/1000 for a,b in pulses(signals['SAMPLE'])]
    periods=[(b-a)/1000 for a,b in zip(starts,starts[1:])]
    # These are observed phases of this bring-up, not complete S1/S2/S3 acceptance.
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    known_trace=digest=='3a06d3f8c230ba9b3aa1b6883f0ae506d0ecef0c3a409caf7afa84fe6d8e2e5a'
    regions={}
    phases=[('bringup_idle',.1,8.529),('after_stats',8.529,15.979),('armed_partial',16.6,1e30)] if known_trace else []
    for name,start,end in phases:
        selected=[(a,b) for a,b in pulses(signals['SAMPLE']) if a>=start*1e9 and b<end*1e9]
        rises=[a for a,b in selected]
        intervals=[(b-a)/1000 for a,b in zip(rises,rises[1:])]
        regions[name]={'width':summary([(b-a)/1000 for a,b in selected]),'period':summary(intervals),
            'jitter':summary([x-20000 for x in intervals]),'skipped_from_intervals':sum(max(0,int(x/20000)-1) for x in intervals)}
    heartbeat=[t for t,v in signals.get('HEARTBEAT',[]) if t>100_000_000]
    store=[{'start_us':a/1000,'duration_us':(b-a)/1000} for a,b in pulses(signals.get('STORE',[]))]
    # Explicitly retain firmware STAT lines and compare only equivalent windows.
    stats=[]
    for line in text.splitlines():
        if line.startswith(('LB PERF','LB STAT','LB RELEASE','LB PERSIST','LB COMMAND','LB STARTUP','LB LATENCY','LB FLASH')):
            stats.append(line)
    matched=None
    if known_trace:
        window=list(pulses(signals['SAMPLE']))[:422]
        ws=summary([(b-a)/1000 for a,b in window])
        matched={'gpio_width':ws,'firmware_stat':{'n':422,'min_us':1114,'avg_us':1149,'max_us':1155},
                 'average_wrapper_overhead_us':1149-ws['avg_us'],
                 'gpio_jitter':summary([(window[i+1][0]-window[i][0])/1000-20000 for i in range(421)]),
                 'note':'Exact known bring-up trace only; not applied to a new stats-reset window.'}
    info={'source':str(path),'sha256':digest,
          'timescale_ns':1,'capture_end_s':max(t for e in signals.values() for t,v in e)/1e9,
          'banner':next((x for x in text.splitlines() if x.startswith('LB BOOT')),None),
          'sample_width':summary(widths),'sample_period':summary(periods),
          'heartbeat_period':summary([(b-a)/1000 for a,b in zip(heartbeat,heartbeat[1:])]),
          'store_pulses':store,'regions':regions,'matched_first_stat_window':matched,'firmware_lines':stats,
          'uart_bytes':len(decoded),'guard_events':re.findall(r'LB EVENT[^\r\n]+',text)}
    out.with_suffix('.json').write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:v for k,v in info.items() if k not in ('firmware_lines','guard_events')},ensure_ascii=False,indent=2))
    return info

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('vcd',type=Path);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();analyze(args.vcd,args.output)
