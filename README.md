# Lockbox

Навчальна модель контролю шафки, Омельченко Богдан Юрійович, ІО-32. STM32C031C6, ST HAL, одна прошивка читає MPU 6050, LDR та PIR, має кнопку охорони, SHOCK/LIGHT, UART і журнал RAM.

ЛР3 у `feature/lab-3`: [актуальний стан і S2](docs/lab3-status-20261101.md), [задачі](docs/tasks.md), [історичні результати](docs/lab3-report.md), [обмеження](docs/limitations.md), [базова лінія](docs/baseline-lab4.md). Source `484fae964f8e7cb1`: S1, одне вікно S2 і S3 з 40 модельованими натисканнями виміряні у Wokwi; фізичні випробування та частина відмовних перевірок відкриті. У S2 зафіксовано пропущені releases та невдалу перевірку flash erase, тому сталий журнал не підтверджено. HAL SysTick має крок 1 мс; CPU load і sub-ms WCET поточним методом недоступні. Результати цифрової моделі не є фізичними вимірюваннями.

## Зібрати й запустити

```sh
make clean
make build
make test
make sanitize
make coverage
```

Docker image `lockbox-lab2-build:local`, build outputs `firmware/build/lockbox.elf`, `.hex`, `.bin`. Wokwi у VS Code: відкрити саме цю папку, команда `Wokwi: Start Simulator`. UART 115200 8N1, self-test починається `LB BOOT project=Lockbox lab=2`. На сайті вставити власний diagram.json, команда `Upload Firmware and Start Simulation…`, вибрати HEX. Після Stop завантажити HEX знову: звичайні Start або Restart сайту можуть компілювати sketch шаблону.

## Перевірити охорону

Початково LDR 10 lux, MPU x=y=0,z=1 g. Натискання MODE понад 30 ms або `arm` починає стабільне калібрування ~500 ms. Після ARMED змінити accelX на 0.4 g: SHOCK. LDR 10→501 lux: LIGHT. Обидві причини зберігаються до MODE або `disarm`. PIR `Simulate motion` лише записує MOTION_ON/OFF. PA5 показує heartbeat, PB5 — тривогу.

UART commands з LF: `status`, `arm`, `disarm`, `probe`, `nack`, `journal`, `help`, `imu 69`, `imu 68`. Wrong sampling address 69 дає контрольований fault; 68 — recovery без скидання тривоги. RFC2217 лише у VS Code, localhost:4007. Див. `python3 tools/monitor.py --help`; потрібний pyserial, без токенів у Git.

## Докази й межі

Власний Wokwi-запуск підтвердив SHOCK/LIGHT/cause 3, PIR, NACK, fault/recovery, disarm і 9 records. 10 груп host tests та sanitizers пройшли на Linux/GCC і macOS/Clang. Sensors/Guard мають 100% line coverage, Guard branches 97.37% GCC.

I2C SDA read annotations не збігаються з CPU/UART raw bytes. Причина не встановлена, правильність фізичного read не підтверджена. Це відкрите обмеження, а не замінені очікуваними байтами дані. Фізичного стенда й вимірів струму немає. STM32C031C6 ще треба погодити замість F411RE/F446RE. Docker apt archive не snapshot, тому байтова незмінність через рік не гарантована.

- [PRD](docs/PRD.md)
- [Піни, монтаж, бюджет, альтернативи](docs/pinout.md)
- [Компоненти](docs/hardware-components.md)
- [Архітектура](docs/architecture.md)
- [Справжні записи, timings і аномалії](docs/captures/analysis.md)
- [Локальна валідація](docs/validation/README.md)
- [Результати ЛР 2](docs/lab2-report.md)
- [10-комітний план](docs/lab2/plan.md)

ЛР 1 локально fast-forward merged у main 9d6190a. Робота ЛР 2 у feature/lab-2; remote main не змінено, PR користувач створює сам. Firmware походить із офіційного [Wokwi ST example](https://github.com/wokwi/stm32-hello-wokwi), ліцензії ST/CMSIS збережено. Незалежний driver/protocol/state machine/captures не скопійовані з MuseGuard.
