import serial,time
with serial.serial_for_url('rfc2217://localhost:4013',baudrate=115200,timeout=.05) as p,open('uart.raw','wb') as f,open('commands.txt','w') as log:
 p.write(b'arm\n');log.write('arm on connection; host monotonic %.6f\n'%time.monotonic());log.flush();start=time.monotonic()
 while time.monotonic()-start<35:
  x=p.read(4096)
  if x:f.write(x);f.flush()
