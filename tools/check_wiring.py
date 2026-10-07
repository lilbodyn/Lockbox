#!/usr/bin/env python3
"""Check the concrete net list, GPIO declarations, ADC channel and MCU model."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
d=json.loads((root/'diagram.json').read_text())
edges=[set(connection[:2]) for connection in d['connections']]
expected={'imu:SCL':'PB8','imu:SDA':'PB9','light:AO':'PA0','nearby:OUT':'PA10',
          'mode:1.l':'PA1','alarm:A':'PB5'}
for device,pin in expected.items():
    assert {device,'nucleo:'+pin} in edges,(device,pin)
assert {"imu:AD0","nucleo:GND.1"} in edges
assert next(p for p in d['parts'] if p['id']=='nucleo')['type']=='board-st-nucleo-c031c6'
ioc=(root/'firmware/lockbox.ioc').read_text()
for declaration in ('PA0.Signal=ADC1_IN0','PA1.Signal=GPIO_Input','PA10.Signal=GPIO_Input',
                    'PB5.Signal=GPIO_Output','PB8.Signal=I2C1_SCL','PB9.Signal=I2C1_SDA'):
    assert declaration in ioc,declaration
for i,pin in enumerate(['PB8','PB9','PA2','PA10','PA4','PA1','PA5','PB5']):
    assert {f'logic:D{i}',f'nucleo:{pin}'} in edges
assert len(set(expected.values()))==len(expected)
print('PASS Lockbox simultaneous wiring, analyzer channels and .ioc')
