import serial,time,json
from pathlib import Path
with serial.serial_for_url('rfc2217://localhost:4013',baudrate=115200,timeout=.05) as p,open('uart.raw','wb') as f,open('collector.jsonl','w') as log:
 start=time.monotonic(); commands=[(0,'status'),(1,'arm'),(35,'status'),(75,'status')];i=0
 while time.monotonic()-start<180:
  t=time.monotonic()-start
  if i<len(commands) and t>=commands[i][0]:
   p.write((commands[i][1]+'\n').encode());log.write(json.dumps({'t':t,'tx':commands[i][1]})+'\n');log.flush();i+=1
  b=p.read(4096)
  if b:f.write(b);f.flush()
