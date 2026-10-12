from pathlib import Path
import sys, argparse, hashlib
sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':11,'axes.spines.top':False,'axes.spines.right':False})
parser=argparse.ArgumentParser(description='Plot the actual documented Lockbox integration capture.')
parser.add_argument('vcd',type=Path)
parser.add_argument('--output',type=Path,default=Path('docs/lab2'))
args=parser.parse_args()
p=args.vcd
if hashlib.sha256(p.read_bytes()).hexdigest() != 'afdd5daee21370ead5db2d974edf42ee63862e4222a37c7b32e987652e85848f':
    parser.error('Input must be the documented integration capture; timestamps are specific to this run.')
_,_,sig=capture.load_vcd(p)
out=args.output
out.mkdir(parents=True,exist_ok=True)
def wave(ax,name,start,end):
 edges=[(start,capture.level(sig[name],start))]+[(t,v) for t,v in sig[name] if start<t<end]+[(end,capture.level(sig[name],end))]
 x=[(t-start)/1e6 for t,v in edges];y=[v for t,v in edges]
 ax.step(x,y,where='post',linewidth=1.2,color='#145b8c');ax.set_ylim(-.15,1.4);ax.set_yticks([0,1]);ax.set_ylabel(name);ax.grid(axis='x',alpha=.2);ax.set_xlim(0,(end-start)/1e6)
fig,axs=plt.subplots(2,1,figsize=(10.8,4.2),sharex=True,constrained_layout=True)
a,b=100960000000,101250000000
wave(axs[0],'BUTTON',a,b);wave(axs[1],'ALARM',a,b)
axs[0].annotate('Дребезг: 2.40 ms',xy=(8.99,.25),xytext=(30,1.05),arrowprops={'arrowstyle':'->'})
axs[1].annotate('Стабільний low → ALARM off: 33.001 ms',xy=(42,.2),xytext=(60,1.1),arrowprops={'arrowstyle':'->'})
axs[1].set_xlabel('Час від 100.960 s, ms');fig.suptitle('Реальний VCD Lockbox: кнопка знімає охорону')
fig.savefig(out/'button-vcd.png',dpi=170);plt.close(fig)
fig,axs=plt.subplots(2,1,figsize=(10.8,3.9),sharex=True,constrained_layout=True)
a,b=480000000,1020000000
wave(axs[0],'ADC_MARK',a,b);wave(axs[1],'HEARTBEAT',a,b)
axs[0].text(70,1.1,'Період ADC: 20.0023 ms; width median: 74.85 µs')
axs[1].set_xlabel('Час від 0.480 s, ms');fig.suptitle('Реальний VCD: ADC і heartbeat у спільній прошивці')
fig.savefig(out/'adc-vcd.png',dpi=170);plt.close(fig)
fig,ax=plt.subplots(figsize=(10.8,3.2),constrained_layout=True)
a,b=10130000,11300000
wave(ax,'UART_TX',a,b)
u=[(t,c) for t,c in capture.decode_uart(sig['UART_TX']) if a<=t<=b]
for t,c in u:ax.text((t-a)/1e6+.012,1.16,f'{c:02X}',fontsize=10)
ax.set_xlabel('Час від 10.130 ms, ms');fig.suptitle('UART 115200 8N1: 4C 42 20 42 4F 4F 54 = "LB BOOT"')
fig.savefig(out/'uart-vcd.png',dpi=170);plt.close(fig)
fig,ax=plt.subplots(figsize=(10.8,2.8),constrained_layout=True)
a,b=73730000000,74780000000
wave(ax,'PIR',a,b);ax.text(150,1.17,'High: 1.000000025 s; event UART через 3.0784 ms')
ax.set_xlabel('Час від 73.730 s, ms');fig.suptitle('PIR записує спостереження; alarm cause не змінюється')
fig.savefig(out/'pir-vcd.png',dpi=170);plt.close(fig)
