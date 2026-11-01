import serial,time,json,hashlib
from pathlib import Path
start=time.monotonic();schedule=[(0,'help'),(1,'status'),(16,'arm'),(20,'status')]+[(26+i*.5,'journal') for i in range(24)]+[(40,'stats'),(44,'status'),(52,'status')];i=0
with serial.serial_for_url('rfc2217://localhost:4017',baudrate=115200,timeout=.03) as port,open('uart.raw','xb') as raw,open('commands.jsonl','x') as log:
 while time.monotonic()-start<60:
  now=time.monotonic()-start
  if i<len(schedule) and now>=schedule[i][0]:
   command=schedule[i][1];port.write((command+'\n').encode());log.write(json.dumps({'host_elapsed_s':now,'command':command})+'\n');log.flush();i+=1
  b=port.read(4096)
  if b:raw.write(b);raw.flush()
print('complete: 60 s host collection; select MCU/VCD-clock windows independently')
