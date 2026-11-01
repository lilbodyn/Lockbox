#!/usr/bin/env python3
"""Plot a real SAMPLE interval from the current product's preserved VCD."""
from pathlib import Path
import hashlib
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from capture import load_vcd
root=Path(__file__).resolve().parents[1]
source=root/'docs/captures/lab3-bringup-14ab26d1.vcd'
_,_,signals=load_vcd(source)
start,end=8.501e9,8.584e9
origin=8.529939425e9
fig,axes=plt.subplots(2,1,figsize=(7.2,3.6),sharex=True,gridspec_kw={'height_ratios':[2,1]})
for ax,name in zip(axes,['SAMPLE','UART_TX']):
    edges=signals[name]
    previous=[v for t,v in edges if t<=start][-1]
    samples=[(start,previous)]+[(t,v) for t,v in edges if start<t<end]+[(end,[v for t,v in edges if t<=end][-1])]
    ax.step([(t-origin)/1e6 for t,v in samples],[v for t,v in samples],where='post',color='#16465e',linewidth=1)
    ax.set_yticks([0,1]);ax.set_ylim(-.15,1.55);ax.set_ylabel(name,fontsize=10)
    ax.grid(axis='x',alpha=.2);ax.spines[['top','right']].set_visible(False)
gap=(8.563660700e9-origin)/1e6
axes[0].annotate('',xy=(0,1.3),xytext=(gap,1.3),arrowprops={'arrowstyle':'<->','color':'#9c2b22'})
axes[0].text(gap/2,1.37,'33.721275 мс між стартами',ha='center',fontsize=10,color='#9c2b22')
axes[0].axvline(20,color='#888888',linestyle=':',linewidth=1)
axes[0].text(20.6,.45,'20 мс',fontsize=9,ha='left',va='center',color='#666666')
axes[1].set_xlabel('Час від SAMPLE у 8.529939425 с, мс',fontsize=10)
fig.suptitle('Lockbox: перерва опитування під час stats',fontsize=12)
fig.text(.13,.025,f'Wokwi VCD, source 14ab26d1; SHA256 {hashlib.sha256(source.read_bytes()).hexdigest()[:16]}…',fontsize=8)
fig.subplots_adjust(left=.13,right=.985,top=.86,bottom=.19,hspace=.20)
out=root/'docs/lab3';out.mkdir(exist_ok=True)
fig.savefig(out/'stats-gap.png',dpi=220)
print(out/'stats-gap.png')
