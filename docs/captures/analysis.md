# Власні записи Lockbox

Усі VCD отримано власним запуском STM32C031C6 у Wokwi 13.10.2026. Повні записи лежать у Lab02/Work/captures, SHA256 і розміри в manifest.json. Скорочені VCD у цьому каталозі містять коментар original_sha256, offset_ns та end_ns. Початкові рівні й реальні переходи збережено. Вирізки не є новими вимірюваннями.

## Параметри й відтворення

VCD має timescale 1 ns. Це роздільність часових позначок симулятора, не частота фізичного sampler. Аналізатор triggerMode=off, bufferSize=3000000 переходів. Канали D0 SCL/PB8; D1 SDA/PB9; D2 UART_TX/PA2; D3 PIR/PA10; D4 ADC_MARK/PA4; D5 BUTTON/PA1; D6 HEARTBEAT/PA5; D7 ALARM/PB5. Вхідна земля спільна.

Для SR імпорт sigrok-cli 0.7.2 `-I vcd:downsample=50` дає 20 MHz. Записи починаються разом із симуляцією. I2C decoder: `i2c:scl=SCL:sda=SDA`; UART: `uart:rx=UART_TX:baudrate=115200`. `*.decode.txt` є справжнім експортом sigrok, `*.uart.txt` незалежним декодуванням capture.py. `*.pvs` налаштовує перегляд, не змінює сигналів.

Команди `probe` і `nack` із LF повторюють WHO_AM_I/6-byte acceleration та звернення 0x69. `imu 69` задає постійну контрольовану помилку, `imu 68` повертає адресу 0x68. AD0 у схемі залишається GND. I2C driver read бере лише 0x3B…0x40, не температуру/gyro.

## I2C і аномалії

Початковий HAL_Mem_Read (hal-bringup-failure-full, SHA 64836310bffb4fe7a51b55634521b1fa051ee45b5c85b9ce2d44a8dfca47cc35) передав address 0x68 W і register 0x75 з ACK, потім не завершив читання. UART показав BUS_ERROR. STOP+HAL_Master_Receive також не дав робочого самотесту. Адаптер регістрів Board задає CR2 цілком і витримує 110 us після адреси/кожного записаного байта, потім читає RXDR одразу. Це обходить ранній TXIS моделі. Власний sensor driver від HAL не залежить.

Запис guard-rx-interrupt-full, SHA afdd5daee21370ead5db2d974edf42ee63862e4222a37c7b32e987652e85848f, дав працездатний self-test і всі демонстрації нижче. SCL median period 10000 ns, тобто 100.000 kHz. WHO transfer від START 4.127100 ms до STOP 4.577875 ms займає 450.775 us. CPU/UART повертає ID=0x68, raw=00,00,00,00,20,00, z=1000 mg. Проте I2C decoder на SDA повертає ID=EB і acceleration FF,FF,FF,FF,EF,FF. У probe ms=94249 UART raw=0C,CC,00,00,20,00, x=399 mg; SDA decoder FD,DD,FF,FF,EF,FF. Це справжнє невирішене розходження між моделлю периферії й лініями. Не трактуємо його як успішну фізичну перевірку read bytes. Правильність перетворення даних підтверджують host tests та UART моделі. Потребу перевірки на фізичному стенді залишено явно.

Прочитати RXDR із затримкою 110 us не допомогло: 6-byte read дав TIMEOUT. GPIO open-drain експеримент дав TIMEOUT SCL. Без зовнішніх pull-up ID на SDA лишився EB. Ці проби не увійшли у фінальний код. Причина спотворення read wire bytes не встановлена; твердження про дефект саме конкретного компонента Wokwi було б передчасним.

Контрольований NACK: `nack`, ms=63390, адреса 0x69 при AD0=0. Експорт i2c-mpu-nack.decode.txt містить START, address write 69, NACK, STOP. Після цього звичайне sampling 0x68 продовжується. Адресний NACK від відсутнього slave відрізняється від нормального NACK master після останнього read byte.

Ручне декодування: START є спадом SDA при high SCL. Вісім рівнів SDA на наступних фронтах SCL `11010000` дають wire byte D0, 7-bit address 68 і R/W=0. Дев'ятий low є ACK. Наступні `01110101` дають register 75, дев'ятий low ACK. Після повторного START `11010001` дає D1, address 68, R/W=1. Це зіставлено зі справжніми Bits у PulseView. Спотворений read EB навмисно не замінено на очікуваний 68.

## UART

115200 8N1. Виміряний одиничний bit 8.6875 us відповідає 48 MHz/BRR417, 115107.9 baud, похибка -0.08%. GPIO VCD містить реальні UART byte times. `LB SELFTEST`, `LB EVENT`, `LB RECORD`, `LB STATUS` розбираються за key=value. Вихід RAM ring 4096 bytes, RX IRQ ring 128 bytes. В інтеграційному запуску dropped=0, rx_errors=0. Ring overflow і UART framing/overrun збільшують rx_errors, поточний рядок відхиляється до LF, щоб частина команди не виконалась.

## GPIO й ADC

ADC_MARK median period 20.002325 ms. Width min 69.525 us, median 74.850 us, max 74.925 us. Це сукупний HAL start/poll/read/stop, не лише conversion 14.417 us. Heartbeat interval близько 500 ms, повний blink cycle 1 s. ADC сам має analog input, прямого цифрового протоколу немає.

У спокої x=y=0, z=1000 mg, norm=1 g. LDR 10 lux дає raw3413 і 2750 mV; 501 lux дає raw999 і 805 mV. Це модель divider, не вимір lux calibration фізичного модуля.

PIR rising=73.741532075 s, falling=74.741532100 s, high width=1.000000025 s. Перший байт MOTION_ON виходить у 73.744610475 s, через 3.078400 ms після входу. PIR не додає alarm cause. Button перший low=100.966597175 s, останній bounce low=100.968996575 s. ALARM падає у 101.001997525 s, тобто 35.400350 ms від першого low або 33.000950 ms від стабільного low. Це узгоджується з debounce 30 ms і оновленням виходу в sampling loop. SHOCK event ms=26701, ALARM rise=26.705566225 s. Різниця 4.566225 ms містить quantization HAL_GetTick; вона не є затримкою від руху миші до удару.

## Реальна послідовність інтеграції

20223 ms ARMING; 20721 ARMED, 498 ms stable calibration. 26701 SHOCK, ax399/cause1. 53641 LIGHT, ADC999/cause3. 63390 NACK diagnostic. 73741 MOTION_ON/cause3; 73860 IMU_FAULT; 74760 MOTION_OFF; 78223 IMU_RECOVERED/cause3. 100998 DISARMED/cause0. `journal` повернув дев'ять записів у правильному порядку.

Host vectors у tests/fixtures/captured_acceleration.h взято з реально декодованих UART payloads, не з невідповідних SDA read annotations. Fake bus відтворює ці 6-byte frame, tests перевіряють mg. Послідовність register writes 6B00,1A03,1909,1C08 окремо перевіряє recording mock. Синтетичні boundary vectors для threshold349/350 і599/600 прямо позначені тестами, не записами симулятора.
