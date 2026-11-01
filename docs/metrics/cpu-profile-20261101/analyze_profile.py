#!/usr/bin/env python3
"""Integrate actual task marker pulses in fixed VCD-time windows; not full core utilization."""
import argparse,json,hashlib,re
from pathlib import Path
from capture import load_vcd,decode_uart
p=argparse.ArgumentParser();p.add_argument('vcd',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
_,_,sig=load_vcd(args.vcd); lines=[];b=bytearray();first=None
for t,v in decode_uart(sig['UART_TX']):
 if first is None:first=t
 b.append(v)
 if v==10:lines.append((first,b.decode('ascii','replace').strip()));first=None;b.clear()
boot=[x for t,x in lines if x.startswith('LB BOOT ')]
if len(boot)!=1 or 'diag-cpu-only' not in boot[0]:raise SystemExit('missing or unexpected diagnostic boot')
arm=[t for t,x in lines if x.startswith('LB EVENT ') and 'type=ARMING' in x]
headers=[t for t,x in lines if x.startswith('LB JOURNAL ')]
if len(arm)!=1 or len(headers)<10:raise SystemExit('missing ARMING or insufficient journal headers')
quiet=(2_000_000_000,arm[0]-1_000_000_000);busy=(headers[0],headers[-1])
if min(b-a for a,b in (quiet,busy))<8_000_000_000:raise SystemExit('windows too short')
def metric(name,a,b):
 e=sig[name];last=0;high=False;total=0;count=0;longest=0
 for t,v in e:
  if t>b:break
  if high:
   d=max(0,min(t,b)-max(last,a));total+=d
   if d:count+=1;longest=max(longest,d)
  high=bool(v);last=t
 if high:
  d=max(0,b-max(last,a));total+=d
  if d:count+=1;longest=max(longest,d)
 return {'high_s':total/1e9,'duration_s':(b-a)/1e9,'pct':100*total/(b-a),'intervals':count,'max_interval_us':longest/1000 if count else None}
results={}
for label,(a,b) in {'quiet_off':quiet,'armed_export':busy}.items():
 results[label]={'start_s':a/1e9,'end_s':b/1e9,'TASK':metric('TASK',a,b),'SAMPLE':metric('SAMPLE',a,b),'STORE':metric('STORE',a,b)}
 if results[label]['TASK']['high_s']+1e-9<results[label]['SAMPLE']['high_s']: raise SystemExit('TASK marker does not cover SAMPLE')
results.update({'boot':boot[0],'sha256':hashlib.sha256(args.vcd.read_bytes()).hexdigest(),'journal_headers':len(headers),'limitations':'Diagnostic copy only; TASK includes active marked foreground sections, synchronous peripheral polling and interrupt time within markers; excludes empty scheduler polls, serial receive pop/empty checks, interrupts outside markers and other instrumentation overhead. UART wire time after return is excluded; changes to probes perturb timing. Not full core utilization, sub-ms WCET, or S2/S3 measurement.'})
args.output.write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');print(json.dumps(results,indent=2))
