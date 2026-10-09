#!/usr/bin/env python3
"""Read actual Wokwi VCD transitions, crop without inventing edges, and decode UART/I2C."""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
from statistics import median

def load_vcd(path):
    names, signals, current, ticks = {}, {}, {}, 0
    header=[]
    in_header=True
    for line in path.read_text().splitlines():
        if in_header:
            header.append(line)
            if line.startswith('$var'):
                parts=line.split();names[parts[3]]=parts[4];signals[parts[4]]=[]
            if line.startswith('$enddefinitions'):in_header=False
        elif line.startswith('#'):ticks=int(line[1:])
        elif line and line[0] in '01' and line[1:] in names:
            name=names[line[1:]];value=int(line[0])
            if current.get(name)!=value:
                signals[name].append((ticks,value));current[name]=value
    assert any('$timescale 1ns' in line for line in header),'expected Wokwi 1 ns VCD'
    return header,names,signals

def level(edges,tick):
    index=bisect.bisect_right(edges,(tick,2))-1
    return edges[index][1] if index>=0 else 0

def decode_uart(edges):
    bit=1e9/115200
    decoded=[];busy_until=-1
    for tick,value in edges:
        if value!=0 or tick<busy_until:continue
        if level(edges,int(tick+0.5*bit))!=0:continue
        byte=sum(level(edges,int(tick+(1.5+i)*bit))<<i for i in range(8))
        if level(edges,int(tick+9.5*bit))!=1:continue
        decoded.append((tick,byte));busy_until=tick+9.8*bit
    return decoded

def decode_i2c(scl,sda):
    merged=sorted([(t,'SCL',v) for t,v in scl]+[(t,'SDA',v) for t,v in sda])
    levels={'SCL':1,'SDA':1};active=None;bits=[];transactions=[]
    for tick,name,value in merged:
        old=levels[name];levels[name]=value
        if name=='SDA' and old!=value and levels['SCL']==1:
            if value==0:
                if active is not None:
                    active['end_ns']=tick;active['end']='RESTART';transactions.append(active)
                active={'start_ns':tick,'bytes':[]};bits=[]
            elif active is not None:
                active['end_ns']=tick;active['end']='STOP';transactions.append(active);active=None;bits=[]
        if name=='SCL' and old==0 and value==1 and active is not None:
            bits.append(levels['SDA'])
            if len(bits)==9:
                byte=0
                for b in bits[:8]:byte=(byte<<1)|b
                active['bytes'].append({'value':byte,'ack':bits[8]==0,'time_ns':tick});bits=[]
    if active is not None:
        active['end_ns']=merged[-1][0]
        active['end']='INCOMPLETE'
        transactions.append(active)
    return transactions

def crop(path,out,start,end):
    header,names,signals=load_vcd(path); reverse={v:k for k,v in names.items()}
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    lines=header[:-1]+[f'$comment original_sha256={digest} offset_ns={start} end_ns={end} $end',header[-1],'#0']
    changes=[]
    for name,edges in signals.items():
        lines.append(f'{level(edges,start)}{reverse[name]}')
        changes.extend((t-start,reverse[name],v) for t,v in edges if start<t<=end)
    last=None
    for tick,symbol,value in sorted(changes):
        if tick!=last:lines.append(f'#{tick}');last=tick
        lines.append(f'{value}{symbol}')
    out.write_text('\n'.join(lines)+'\n')

def analyze(path,out):
    _,_,signals=load_vcd(path)
    uart=decode_uart(signals['UART_TX'])
    text=bytes(b for _,b in uart).decode('ascii',errors='replace')
    Path(str(out)+'.uart.txt').write_text(text)
    transactions=decode_i2c(signals['SCL'],signals['SDA'])
    rising=[t for t,v in signals['SCL'] if v==1]
    periods=[b-a for a,b in zip(rising,rising[1:]) if 5000<=b-a<=20000]
    adc_on=[t for t,v in signals['ADC_MARK'] if v==1]
    heartbeat=[t for t,_ in signals['HEARTBEAT'] if t>0]
    info={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
          'uart_bytes':len(uart),'scl_period_ns':median(periods) if periods else None,
          'adc_period_ns':median([b-a for a,b in zip(adc_on,adc_on[1:])]) if len(adc_on)>1 else None,
          'heartbeat_intervals_ns':[b-a for a,b in zip(heartbeat,heartbeat[1:])],
          'i2c':transactions,'signals':{k:v for k,v in signals.items() if k not in ['SCL','SDA','UART_TX']},
          'uart_byte_times_ns':[[t,b] for t,b in uart]}
    Path(str(out)+'.json').write_text(json.dumps(info,indent=2)+'\n')
    print(json.dumps({k:v for k,v in info.items() if k not in ['i2c','signals','uart_byte_times_ns','heartbeat_intervals_ns']}))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--start',type=float);parser.add_argument('--end',type=float)
    args=parser.parse_args()
    if args.start is not None:
        assert args.end is not None and args.end>args.start
        crop(args.source,args.output,int(args.start*1e9),int(args.end*1e9))
    else:analyze(args.source,args.output)
