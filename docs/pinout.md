# Підключення Lockbox

Джерела: [DS13867 Rev 4](https://www.st.com/resource/en/datasheet/stm32c031g6.pdf), табл. 12, с. 31–34, табл. 13, с. 35, табл. 15, с. 36; [UM2953 Rev 2](https://www.st.com/resource/en/user_manual/um2953-stm32-nucleo64-board-mb1717-stmicroelectronics.pdf), рис. 7, с. 19, табл. 11, с. 20; [RM0490](https://www.st.com/resource/en/reference_manual/dm00781702-.pdf), розділи RCC/GPIO/ADC/I²C/USART. Конфігурація стосується STM32C031C6 у LQFP48, не F411/F446.

| Пін | Режим / AF | Блок і сигнал | Arduino / morpho | Пристрій | Напруга, pull | Причина |
| --- | --- | --- | --- | --- | --- | --- |
| PB8 | AF6 open-drain | I²C1 SCL | D15 CN5.10 / CN10.3 | MPU SCL | 3.3 В, зовнішній 4.7 kΩ | Збережено шину ЛР1, DS табл.15 |
| PB9 | AF6 open-drain | I²C1 SDA | D14 CN5.9 / CN10.5 | MPU SDA | 3.3 В, зовнішній 4.7 kΩ | Пара PB8, DS табл.15 |
| PA0 | analog, без AF | ADC1 IN0 | A0 CN8.1 / CN7.28 | LDR AO | 0…3.3 В, без pull | Окремий ADC, DS табл.12 |
| PA10 | input, без AF | GPIO | D2 CN9.3 / CN10.33 | PIR OUT | очікується 3.3 В, MCU pull-down | Не займає PA2/PA3, UM табл.11 |
| PA1 | input, без AF | GPIO | A1 CN8.2 / CN7.30 | MODE 1.l | active low, MCU pull-up | Окремо від RESET і від ADC PA0 |
| PB5 | output push-pull | GPIO | D6 CN9.7 / CN10.29 | ALARM анод | 3.3 В, LED+1 kΩ до GND | Незалежно від heartbeat |
| PA4 | output push-pull | GPIO | A2 CN8.3 / CN7.32 | ADC_MARK | 3.3 В, без pull | Вимір періоду ADC |
| PA5 | output push-pull | GPIO | D13 CN5.6 / CN10.11 | LD4 heartbeat | штатний LED | Збережено FR-02 ЛР1 |
| PA2 | AF1 push-pull | USART2 TX | VCP / CN7.35 | monitor RX | 3.3 В | Збережено UART ЛР1, DS табл.13 |
| PA3 | AF1 | USART2 RX | VCP / CN7.37 | monitor TX | 3.3 В | Команди, DS табл.13 |

У UM2953 табл.12 назви SCL/SDA біля PB8/PB9 переставлені відносно DS табл.15 і UM табл.11. Розводимо за функціями кристала: PB8=SCL, PB9=SDA. Для точок morpho використовуємо рис.7, для Arduino табл.11.

## Послідовність монтажу

1. З'єднати GND плати, MPU, LDR, PIR і аналізатора. У Wokwi точні назви плати `GND.1`, `3V3.1`, `5V.1`.
2. MPU VCC та LDR VCC до 3V3. PIR VCC до 5V. У фізичному аналогу перед підключенням перевірити OUT PIR і AO LDR; виробник моделей Wokwi не задає конкретну фізичну плату.
3. MPU AD0 до GND, SCL до PB8, SDA до PB9; підтяжки 4.7 kΩ до 3V3. INT/XCL/XDA лишити вільними. На реальному breakout спочатку врахувати його власні резистори.
4. LDR AO до PA0, DO вільний. PIR OUT до PA10. Кнопка 1.l до PA1, 2.l до GND. Утримання з'єднує вхід із землею, RESET плати не перемикає режим.
5. PB5 до анода червоного LED; катод через 1 kΩ до GND. PA5 керує штатним LD4.
6. Serial monitor TX до PA3, RX до PA2. D0…D7 аналізатора відповідно PB8, PB9, PA2, PA10, PA4, PA1, PA5, PB5.

| Шина | Адреса / ID | Налаштування |
| --- | --- | --- |
| I²C1 MPU6050 | AD0=0, 7-bit 0x68; WHO_AM_I 0x75=0x68 | Standard mode 100 kHz; 0x6B=0, 0x1A=3, 0x19=9, 0x1C=8; accel ±4 g |
| ADC1 | ID-регістру немає | PA0, 12 bit, 20 мс, software trigger |
| GPIO PIR/кнопка | ID-регістру немає | PIR high/low, MODE active low |
| USART2 | 115200 8N1 | Журнал і точні текстові команди з LF |

## Розрахунок частот

HSI=SYSCLK=HCLK=PCLK=48 MHz. I²C TIMINGR ЛР1 `0x20303E5D`: PRESC=2, SCLDEL=3, SDADEL=0, SCLH=62, SCLL=93. Tick timing=(2+1)/48 MHz=62.5 ns; programmed high+low=(63+94)×62.5 ns=9.8125 µs. Повний період містить синхронізацію, фільтр та фронти; його вимірюємо окремо. UART BRR≈417, bit≈8.6875 µs, actual baud≈115108. Теоретична помилка від 115200 приблизно −0.08% без урахування HSI.

ADC clock=PCLK/4=12 MHz. Sampling=160.5 cycles, conversion=12.5 cycles; 173/12 MHz≈14.4167 µs. LOW-frequency trigger додає rearm cycles. ADC_MARK охоплює HAL start/poll/read/stop; він не є безпосереднім виміром внутрішніх 12.5 циклів.

Для цифрового аналізу імпорт VCD на 20 MHz дає близько 200 відліків на SCL і 174 на UART bit. Wokwi VCD зберігає переходи з роздільністю 1 ns, це не фізичний аналоговий sampler. Буфер 3 млн переходів потребує приблизно 27 MB пам'яті моделі за її документацією. Запис починається разом із симуляцією, без тригера.

## Живлення

MPU6050 має VDD 2.375…3.46 V; для вузла 3V3 резервуємо 10 mA на модуль, а не стверджуємо паспортний peak breakout. LDR divider із 10 kΩ споживає ≤0.33 mA; резерв для модуля 5 mA. Дві підтяжки дають ≤1.405 mA при обох low. LED із 1 kΩ має консервативну верхню межу 3.3 mA, при red Vf≈2 V очікується близько 1.3 mA. Додатковий бюджет 3V3≤19.7 mA. Для PIR на 5V резерв 10 mA є припущенням для майбутнього модуля.

UM2953 §6.5.2 задає 500 mA сумарного регулятора разом із платою. Гарантований запас невідомий без вимірювання струму Nucleo і конкретних модулів. Wokwi не вимірює ці струми, RC rise time, перешкоди й реальну чутливість PIR. Фізичний стенд потребує мультиметра та осцилографа; зовнішній силовий привід відсутній.

## Відхилені розкладки

PA3 для кнопки/PIR відхилено через UART RX. PA5 для LDR відхилено через навантаження LD4. I²C PA9/PA10 AF6 відхилено, оскільки зруйнувало б збережену пару ЛР1 й забрало PA10. Злиття heartbeat і alarm на одному LED відхилено: постійна тривога приховала б ознаку роботи циклу. DMA, PWM, SPI та DCMI не використовуються, SWD PA13/PA14 не зайнято. Немає конфлікту I²C-адрес, бо MPU є єдиним slave.
