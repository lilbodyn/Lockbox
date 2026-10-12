# Компоненти Lockbox після ЛР 2

Усі вузли одночасно працюють в одній схемі та ELF. Піни, адреси, живлення й точні джерела наведено в [pinout.md](pinout.md), дані в [captures/analysis.md](captures/analysis.md).

| Вузол | Фінальна конфігурація | Реальна перевірка моделі |
| --- | --- | --- |
| STM32C031C6 | 48 MHz; PA5 heartbeat; PB5 alarm; USART2 PA2/PA3 | Основний цикл, 10 груп host-тестів, ARM build; заміна плати ще не погоджена |
| MPU6050 | PB8/PB9 AF6, AD0 на GND, 0x68, ±4 g, 8192 LSB/g, опитування 50 Hz | CPU ID 68; у спокої 1 g; X 399 mg; NACK 69; байти SDA EB/FF не збігаються з CPU, причина невідома |
| LDR | 3V3, AO PA0 ADC1 IN0, 12 bit, медіана трьох вимірів | 10 lux дає 3413 counts, 501 lux — 999 counts; LIGHT |
| PIR | 5V, OUT PA10 input pulldown, delay 1 s | Обидва фронти, high 1.000000025 s; під охороною cause=0 |
| MODE | PA1 input pullup, active low, дребезг увімкнений | 33.001 ms від стабільного low до вимкнення ALARM; тести меж та утримання |
| ALARM LED | PB5, червоний LED, 1 kΩ до GND | Світиться при cause 1/2/3, вимикається при disarm |
| Аналізатор | D0…D7: SCL, SDA, UART, PIR, ADC, кнопка, heartbeat, alarm | Власні VCD/SR та декодування з SHA-256 оригіналів |

Документація Wokwi описує **моделі симулятора**, а не паспорт конкретного виробника фоторезистора чи PIR-модуля. У методичці просять сторінки офіційних datasheet. Для MPU 6050 є [реєстрова карта TDK InvenSense](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map.pdf): `WHO_AM_I` описаний на сторінці 45. Для двох узагальнених моделей Wokwi виробник не названий, тому паспорт реального модуля не слід вигадувати. Потрібно або погодити посилання на специфікації симулятора для форми B, або вибрати конкретні фізичні аналоги й чесно зазначити, що в Wokwi їхня поведінка спрощена.

Джерела для віртуальної схеми:

- [Підтримувані STM32 у Wokwi](https://docs.wokwi.com/getting-started/supported-hardware)
- [Nucleo-C031C6: можливості симуляції](https://docs.wokwi.com/parts/board-st-nucleo-c031c6)
- [MPU 6050 у Wokwi](https://docs.wokwi.com/parts/wokwi-mpu6050)
- [Фоторезистор у Wokwi](https://docs.wokwi.com/parts/wokwi-photoresistor-sensor)
- [PIR у Wokwi](https://docs.wokwi.com/parts/wokwi-pir-motion-sensor)
- [Кнопка у Wokwi](https://docs.wokwi.com/parts/wokwi-pushbutton)
