#!/usr/bin/env python3
"""Measure actual BUTTON edges during recorded journal UART and REACTION."""
import argparse
import bisect
import hashlib
import json
import re
from pathlib import Path
from capture import load_vcd, decode_uart

def pair(presses, releases, responses, intervals):
    result=[]
    starts=[a for a,b in intervals]
    for i,t in enumerate(presses):
        end=presses[i+1] if i+1<len(presses) else 2**63-1
        ri=bisect.bisect_right(releases,t)
        release=releases[ri] if ri<len(releases) and releases[ri]<end else None
        r=bisect.bisect_left(responses,t)
        response=responses[r] if r<len(responses) and responses[r]<end else None
        ix=bisect.bisect_right(starts,t)-1
        during=ix>=0 and intervals[ix][0]<=t<=intervals[ix][1]
        result.append({'press_ns':t,'release_ns':release,'response_ns':response,
                       'press_width_ms':(release-t)/1e6 if release else None,
                       'latency_ms':(response-t)/1e6 if response else None,
                       'during_record_tx':during})
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('vcd',type=Path);p.add_argument('--output',required=True,type=Path)
    args=p.parse_args();_,_,signals=load_vcd(args.vcd)
    decoded=decode_uart(signals['UART_TX']); line=[]; intervals=[]; boots=[]
    for t,b in decoded:
        line.append((t,b))
        if b!=10:continue
        text=bytes(c for _,c in line)
        if text.startswith(b'LB RECORD '):intervals.append((line[0][0],int(line[-1][0]+10e9/115200)))
        if text.startswith(b'LB BOOT '):boots.append(text.decode('ascii','replace').strip())
        line=[]
    presses=[t for t,v in signals['BUTTON'] if v==0 and t>0]
    releases=[t for t,v in signals['BUTTON'] if v==1 and t>0]
    responses=[t for t,v in signals['REACTION'] if t>0]
    rows=pair(presses,releases,responses,intervals)
    eligible=[r for r in rows if r['during_record_tx'] and r['press_width_ms'] is not None and r['press_width_ms']>=30]
    measured=[r['latency_ms'] for r in eligible if r['latency_ms'] is not None]
    result={'vcd_sha256':hashlib.sha256(args.vcd.read_bytes()).hexdigest(),
            'boot_lines':boots,'record_tx_lines':len(intervals),'physical_falling_edges':len(rows),
            'eligible_actual_presses_in_record_tx':len(eligible),'eligible_with_response':len(measured),
            'latency_min_ms':min(measured) if measured else None,
            'latency_avg_ms':sum(measured)/len(measured) if measured else None,
            'latency_max_ms':max(measured) if measured else None,'rows':rows,
            'scope':'S3 only if >=30 eligible real edges, response pairing, clock and experiment conditions validated. Missing response is not zero latency.'}
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='rows'},indent=2))
if __name__=='__main__':main()
