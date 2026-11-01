import serial,time
with serial.serial_for_url('rfc2217://localhost:4013',baudrate=115200,timeout=.1) as p,open('uart.raw','wb') as f:
 p.write(b'arm\n');start=time.monotonic()
 while time.monotonic()-start<45:
  b=p.read(4096)
  if b:f.write(b);f.flush()
