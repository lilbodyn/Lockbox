# Базова лінія Lockbox для ЛР4

> Нижче — попередня методика з історичними TIM3-кандидатами. Поточна версія та фактичні S1/S2 наведені у [стані ЛР3](lab3-status-20261101.md); S3 виміряно для поточного source; фізичні випробування та порівняння з ЛР4 залишаються відкритими.

ЛР4 не реалізується. Нижче збережено методику повторення bare-metal сценаріїв. Приймання ЛР3 ще не завершено; порожнє вимірювання не замінюється оцінкою або host-числом.

## Стенд і версії

STM32C031C6, Cortex-M0+, HSI48 48 MHz, UART 115200 8N1, I²C 100 kHz, MPU 0x68 ±4 g, LDR PA0, PIR PA10, MODE PA1, ALARM PB5. Вихідні значення моделі: IMU 0/0/1000 mg, LDR 10 lux → ADC=3413, PIR low. Одна прошивка, без RTOS.

Перевірена попередня вимірювальна версія має source `14ab26d1a2a4730a`; сирий VCD лежить у `docs/captures/lab3-bringup-14ab26d1.vcd`. Банер і незалежно декодований UART містяться в `docs/metrics/bringup-14ab26d1.*`. Точний SHA256 VCD `3a06d3f8c230ba9b3aa1b6883f0ae506d0ecef0c3a409caf7afa84fe6d8e2e5a`.

Кандидат для наступних повних прогонів має source `2c6940d3488ab256`. Його повні хеші, базовий Git revision та хеші ELF/HEX наведено в `docs/metrics/source-manifest.json` і `artifacts.json`. Він ще не пройшов інтерактивні S1/S2/S3. Динамічні числа старої версії не є прийманням цього кандидата.

## Відтворення

1. `python3 tools/stamp_build.py`, ARM build у власному Docker-контейнері. Порівняти source з банером. Після зміни C/H/linker/diagram попередні приймальні вимірювання треба повторити.
2. Запустити саме HEX цього продукту. У браузерному Wokwi після Stop знову використати Upload Firmware and Start Simulation; звичайний Start може зібрати sketch шаблону. Trial не активувати.
3. Для UART automation у VS Code використати вже налаштований RFC2217 localhost:4007. `python3 -m venv .uart-venv`, `.uart-venv/bin/pip install pyserial==3.5`. Скрипт нижче сам симулятор не запускає.
4. Створювати окрему папку кожного реального запуску через `--output`. `--plan-only` створює лише план і не є тестом MCU. Зберегти VCD після Stop разом з raw UART та `run.json`.

```sh
.uart-venv/bin/python tools/lab3_scenarios.py S1 --output docs/metrics/S1-run1
.uart-venv/bin/python tools/lab3_scenarios.py S2 --output docs/metrics/S2-run1
.uart-venv/bin/python tools/lab3_scenarios.py S3 --rate 1280 --output docs/metrics/S3-run1
.uart-venv/bin/python tools/lab3_scenarios.py fault --output docs/metrics/fault-run1
.uart-venv/bin/python tools/lab3_scenarios.py boundary --output docs/metrics/boundary-run1
```

Перед кожним S1/S2/S3 потрібен новий старт MCU, щоб stack high-water належав саме цьому сценарію. `stats reset` скидає статистики й фазу releases, але не переписує painted stack, не очищає Guard і не стирає носій.

| Сценарій | Умови та дії | Критерій і джерело |
|---|---|---|
| S1 | 60 с OFF, без зовнішніх подій | ≥3000 sampling releases; повний STAT/RELEASE, VCD |
| S2 | arm, стабільні сенсори, рідкісний status; 3660 с host schedule | Підтвердження `window_us≥3600000000` на MCU; host тривалість сама по собі недостатня. Перевірити heap, stack, кількість записів, CRC |
| S3 | 90 с; RAM journal заповнюється реальними arm/disarm; безперервні status/journal/probe, періодичний stats | Під час експорту ≥30 натискань MODE з high/low довше debounce; окремо PIR, SHOCK, LIGHT. Зберегти фізичні GPIO та реакцію, не замінювати їх UART-командою |
| Відмова/повернення | `imu 69`, 8 с помилки, `imu 68` | Контрольований адресний fault, heartbeat/ADC живі, recovery, cause не губиться. Це не фізичне SDA short |
| Межа навантаження | sample 20/10/5/2/1 мс по 30 с; UART rates 320/1280/5120/11520 host B/s окремими S3 | Зафіксувати перший фактичний missed/deadline/dropped. Швидкість host запиту не видавати за виміряну wire speed. Діагностичні sample режими не проходять продуктове приймання |
| Старт | Завантажити HEX, запис від t=0 | Перші коректні IMU/ADC, початковий PIR, ready. STARTUP time від початку TIM3, не від фізичного VDD |
| Неготовий сенсор на старті | Окремий стенд з керованим AD0 або живленням, повернення адреси 0x68 | Поки не перевірено на MCU; host fake bus перевіряє помилки init |
| Архів | Події, persist/history, заповнення двох сторінок, reset | CRC, збереження boot/ticket, ротація; успішний flash ще не підтверджений |
| Втрата живлення | Відключення VDD під час program/erase, повторне читання | Лише апаратний стенд доведе поведінку живлення. Host byte-cut tests перевіряють алгоритм, не brownout |

Варіант `lockbox-factory-erased.hex` створюється `tools/factory_hex.py` і явно ініціалізує 4096 резервних байтів значенням FF. Це окремий, поки не запущений експеримент для стартового стану flash у моделі. Він не моделює стирання, не виправляє FLASH peripheral і стирає попередній архів при завантаженні. У run metadata потрібно передати `--firmware` з фактично завантаженим файлом.

## Метрики та межі їх тлумачення

TIM3 1 MHz має 16-bit wrap кожні 65.536 мс. Кандидат накопичує uint16 різниці CNT у foreground і SysTick, без UIF high-word. Між читаннями потрібно строго менше 65.536 мс. Повний пропущений wrap невидимий; max_modulo_step не доводить цю межу. Новий кандидат потребує незалежної VCD перевірки. Підсумковий uint32 time переповнюється за 4294.967296 с; різниці коректні для інтервалів коротших одного такого періоду. Host release test проходить wrap, але не вимірює реальний timer.

Завдання логують min/avg/max. Jitter рахують між фактичними стартами; late і deadline окремо від missed releases. Для VCD використовувати абсолютні 1 ns позначки; це час цифрової моделі, не підтвердження точності фізичного генератора MCU. `analyze_lab3.py` наразі має часові області саме bring-up запису; для нових прогонів загальні метрики коректні, області потрібно задавати за новим журналом подій.

`load_permille` є часткою часу обгорнутих foreground tasks, без idle перевірок, UART pump, IRQ та частини обліку. Busy polling залишає ядро активним; це не повне апаратне CPU utilization і не вимір енергоспоживання. Для порівняння з RTOS повторити обидві метрики окремо.

## Наявні числа

| Метрика | Попередній bring-up 14ab26d1 | Повне S1/S2/S3 кандидата |
|---|---:|---|
| Тривалість VCD | 115.749970625 с | не виконано |
| Опрос, кількість GPIO імпульсів | 5782 | не виконано |
| Міжстартовий період min/avg/max | 6316.825 / 19999.989 / 33721.275 мкс | не виконано |
| Ширина SAMPLE min/avg/max | 1093.225 / 1154.909 / 3768.4 мкс | не виконано |
| Heartbeat max інтервал | 504517.375 мкс | не виконано |
| Stack high-water | 968 B у першому stats | не виконано |
| Heap sbrk peak | 0 B у першому stats | не виконано |
| Постійне збереження | failed=1, ticket=0 після першої спроби erase | не виконано |
| Пам'ять кандидата | flash 26896 B + архів 4096 B; RAM з резервами 10096 B | ARM build, не dynamic run |

У ЛР4 перевірити гіпотези з `limitations.md`, повторити ту саму схему, UART, навантаження, методику й кількість повторів. RTOS не усуває однобанковий flash stall, обмеження моделі Wokwi, пропускну здатність UART або втрату живлення.


## Негативний pilot1fe і новий polling-кандидат

Власний `lab3-pilot-1fe-20261028.vcd` триває 119.942155525 с, SHA256 c1ea6311e7b76a5119f2706cbf931936ac8840db3e1edadb700192dfd1b5a753. Два сусідні STATUS переходи мають MCU delta4295467 мс за VCD500.082325 і499.856850 мс. У тихому фрагменті2–40 с незалежний clock gate отримав offset span4294967.981775 мс,76 рядків, результат fail. Внутрішні STAT/load цього pilot недостовірні; це не CPU latency. Первинна периферійна причина не ізольована.

Erase повернувся до VERIFY_FAIL page1 offset0 word0x00000000 замість FF, SR=CR=0; failed1/ticket0. Код verify виконується після HAL_OK, реєстри зчитані після Lock. Успішний program/CRC/durability не підтверджено; загальна непідтримка моделі не встановлена. Деталі й raw у `docs/metrics/pilot-1fe-20261028-r2/`, шляхи від кореня repo; із цієї папки metrics це `pilot-1fe-20261028-r2/`.

Кандидат2c6940d3488ab256 прибирає UIF/IRQ high-word. Атомарне накопичення raw CNT виконується також із SysTick, щоб HAL startup wait не пропускав читання. Строга умова: інтервал читання<65536 мкс; її треба звірити незалежним VCD. 400000 host counter-кроків через16/32-bit wrap, негативний приклад uint32-backstep, macOS/Linux tests та sanitizers пройшли. Це не MCU перевірка. Діагностика Flash додає HAL status/error/CR і before/after readback. Повторний pilot і приймальні S1/S2/S3 лишаються потрібними.

## Додаткове порівняльне випробування поточного ELF
Схему, стимул і UART див. у `docs/lab3/fault-fixture` та `docs/metrics/fault-20261101`; повний VCD збережений у Lab03/Work. SDA штучно утримувалась LOW 10–13 с модельного часу; зафіксовані IMU_FAULT і IMU_RECOVERED. PIR high/low у OFF — синтетичний сигнал; для ARMED потрібен окремий тест. Не переносити старі Lab2 результати на цей ELF без повторення.
