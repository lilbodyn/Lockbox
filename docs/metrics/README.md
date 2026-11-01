# Вимірювання ЛР3 Lockbox

`bringup-14ab26d1.vcd` у сусідньому captures є власним реальним запуском Wokwi 28.10.2026. `bringup-14ab26d1.uart.txt` отриманий незалежним декодуванням саме UART_TX цього VCD, `*.json` обчислений із реальних фронтів. `*.uart-ui.txt` збережений із серійного монітора перед зупинкою; його останній рядок неповний. Він не редагувався для доповнення відсутніх байтів. Первинний VCD завершується на 115.749970625 с.

Source старого запуску: `14ab26d1a2a4730a0255b31d2c35360b1af4565e48bc5b7a49381d2c78213d72`. Банер містить цей короткий source та базу `4d9a0a0a2777`. Точний версійний знімок збережено локально в Lab03/Work, поза Git-коренем. Цей запуск передує останнім виправленням UART, linker RAM section та діагностики flash.

Кандидат до інтерактивного приймання: `source-manifest.json`, `artifacts.json`; source `2c6940d3488ab256`. `memory-final.txt` і `memory-by-module.json` належать саме йому. Host/sanitize Linux і macOS пройшли; див. `checks-poll16-linux.log`, `host-poll16-macos.log`, `sanitize-poll16-macos.log`. Поточний coverage входить у `checks-poll16-linux.log`, отриманий з чистих profile data та абсолютних source paths. `coverage-final-linux.log` зберігає попередній чистий прогін до ASCII-валідації. Раніше gcov не відкривав відносні source files і бачив timestamp старого archive profile. Це виправлено у tests/Makefile, чистий повтор відтворює наведені відсотки й повні .gcov рядки. `host-final-linux.log`, `host-first.log`, `arm-build-first.log` є попередніми перевірками і не замінюють фінальний log.

## Порівняння двох способів

Перші 422 SAMPLE імпульси VCD відповідають n=422 у першому MCU `STAT sample`. За TIM3 min/avg/max=1114/1149/1155 мкс. GPIO width min/avg/max=1093.225/1107.133/1111.200 мкс. Середня різниця становить 41.867 мкс, 3.78% від GPIO width. Обгортка TIM3 починається до `release_due` і підняття піна та закінчується після опускання. Тому її число включає додатковий scheduler/probe/clock overhead. Це не калібрування відносно фізичного годинника.

Для тих самих 421 міжстартових інтервалів GPIO jitter min/max=−3182.25/+3227.65 мкс, MCU RELEASE sample дає −3182/+3225 мкс. Різниця максимумів 2.65 мкс пов'язана з різними точками timestamp/probe і квантуванням 1 мкс. Із цих чисел не робиться висновок про точність реального HSI48.

Після виконання `stats` спостерігається інтервал 33721.275 мкс між SAMPLE 8.529939425 с та 8.563660700 с. Це довша перерва опитування, а не успішне стирання flash. Одна STORE pulse має 73.5 мкс; UART повідомляє failed=1/ticket=0. Видавати її за фізичний час erase не можна.

## Стан обов'язкових сценаріїв

| Вимога | Підтвердження зараз | Залишок |
|---|---|---|
| S1 | 422 запусків до першої stats, фактичний OFF | Повний новий S1 ≥3000 sampling releases |
| S2 | 4957 GPIO sampling у фрагменті ARMED, приблизно 99 с | ≥1 години MCU time, рідкісні команди, CRC на підтвердженому носії |
| S3 | Вплив однієї stats, відмова спроби erase | Повний одночасний сценарій, ≥30 фізичних подій, UART flood |
| Межа навантаження | План sample/rate sweep | Реальні пороги перших порушень |
| Старт | Перші IMU/ADC за TIM3 79419/79504 мкс; обидва валідні | Повтор на кандидатові, startup-not-ready |
| Відмова/recovery | Відомі ЛР2 та host tests | Новий запуск кандидата, час timeout/NACK, GPIO |
| Пам'ять | ARM sections/map/.su кандидата; paint 968 та heap 0 старої версії | Динамічні S1/S2/S3 кандидата |
| Power loss | 2178 host byte-cut cases, CRC і recovery; не brownout | Фізичне вимкнення під час запису та повторне читання |

На цьому етапі немає підтвердженого максимуму S3. Статуси FR з числовими гарантіями не позначаються як повністю прийняті. Нижні/середні значення не підміняють найгірші.

## Пам'ять кандидата

ARM linker: flash 26896/28672 B у програмному регіоні, ще 4096 B окремо відведено архіву; RAM10096/12288 B з резервами. Секції SRAM `.data`168 + `.bss`7364 =7532 B, heap reserve 512 + stack reserve 2048 + alignment 4 =2564 B; запас 2192 B. `.data` містить 56 B RAM code HAL; conventional `size` класифікує CODE-marked section нетипово, тому RAM береться з адрес секцій/map, а не механічно з колонок `data+bss`.

`.su` дає максимальний власний frame 280 B для `serial_line`, не повну глибину. Ланцюг `main → lockbox_poll → sample_task → guard_observe → light_step/alarm_reason → record → on_event → print_event → serial_line` потребує суми одночасно активних frame, а libc formatting та ISR додають свої. Prebuilt newlib/libgcc не містять .su; повний статичний верхній bound не заявлено. Часткова статична сума шляху через calibrate становить 704 B до libc/IRQ за наявними .su. Це нижня часткова оцінка, а не верхній bound. Старий paint 968 B підтверджує, що один frame 280 B не описує весь стек. `heap_peak` є high-water sbrk, а не mallinfo; у записаному першому вікні дорівнює 0.

Значення `load_permille=109` першого вікна означає 10.9% виміряних foreground задач. Core не виконує WFI і залишається зайнятим polling. Відсоток не включає IRQ/UART pump/частину обліку і не є повним CPU utilization. У майбутній таблиці треба зберегти це визначення й окремо вказати busy-spin.

Stack painting починається першим викликом у main. Ранні Reset_Handler/constructor frame до цього виклику не охоплені; high-water не слід називати повним максимумом усіх фаз старту.


## Негативний pilot1fe і новий polling-кандидат

Власний `lab3-pilot-1fe-20261028.vcd` триває 119.942155525 с, SHA256 c1ea6311e7b76a5119f2706cbf931936ac8840db3e1edadb700192dfd1b5a753. Два сусідні STATUS переходи мають MCU delta4295467 мс за VCD500.082325 і499.856850 мс. У тихому фрагменті2–40 с незалежний clock gate отримав offset span4294967.981775 мс,76 рядків, результат fail. Внутрішні STAT/load цього pilot недостовірні; це не CPU latency. Первинна периферійна причина не ізольована.

Erase повернувся до VERIFY_FAIL page1 offset0 word0x00000000 замість FF, SR=CR=0; failed1/ticket0. Код verify виконується після HAL_OK, реєстри зчитані після Lock. Успішний program/CRC/durability не підтверджено; загальна непідтримка моделі не встановлена. Деталі й raw у `docs/metrics/pilot-1fe-20261028-r2/`, шляхи від кореня repo; із цієї папки metrics це `pilot-1fe-20261028-r2/`.

Кандидат2c6940d3488ab256 прибирає UIF/IRQ high-word. Атомарне накопичення raw CNT виконується також із SysTick, щоб HAL startup wait не пропускав читання. Строга умова: інтервал читання<65536 мкс; її треба звірити незалежним VCD. 400000 host counter-кроків через16/32-bit wrap, негативний приклад uint32-backstep, macOS/Linux tests та sanitizers пройшли. Це не MCU перевірка. Діагностика Flash додає HAL status/error/CR і before/after readback. Повторний pilot і приймальні S1/S2/S3 лишаються потрібними.

Local home paths in historical JSON metadata are replaced by `<LOCAL_HOME>` for portability. Original local metadata is retained with the Lab03 working evidence; UART and VCD bytes are not altered.
