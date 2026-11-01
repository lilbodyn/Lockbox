# Проблеми та виправлення ЛР3

| Симптом | Причина та доказ | Виправлення / залишок |
|---|---|---|
| Лінкер відхилив першу ARM збірку через multiple definition EXTI4_15_IRQHandler | Згенерований Core вже має спільний EXTI4_15 для PC13, вимірювальний Board додав друге визначення | PIR handler виділено в `board_pir_interrupt`, виклик з USER CODE існуючого Core ISR; ARM build пройшов |
| Host cut-test очікував старий ticket при перериванні до 64-го байта, але scanner приймав новий | Останні три байти commit inverse лишаються FF і вже збігаються з erased носієм; валідний, повністю перевірений CRC запис може існувати до формального завершення останньої byte-write у host-моделі | Тест перевіряє семантику "старий або повний новий", жорстко відкидає неповний payload, звіряє всі поля й продовження після reboot; 2178 cut cases проходять |
| Після першого ARMING у Wokwi архів не прийняв запис | Початковий scan бачить 64 невалідних слотів, які не є порожніми за FF-критерієм, ticket0; спроба erase дає failed=1; STORE pulse73.5 мкс. Первинний лог не розрізняє HAL і verify | Додано явні unlock/HAL/verify diagnostics, stop-on-failure та pending/lost counters. Рішення не підтверджено; потрібен новий запуск. Factory-FF HEX є тільки підготовленою перевіркою вихідного стану |
| Діагностичний stats збільшив інтервал між опитуваннями до 33.721275 мс | Послідовне vsnprintf багатьох рядків утримує foreground; команда має MCU latency28.808 мс | Затримка збережена як реальне обмеження; sampler phase не дрейфує, missed/late рахуються. Кооперативний logger лишено гіпотезою, S3 кандидата ще не виконано |
| ARM map розміщував .RamFunc HAL у flash як orphan | Linker script не збирав RAM-function input section у startup-copied SRAM data. Це helper fast programming, поточний writer використовує doubleword mode | Додано .RamFunc до .data, startup копіює його штатно. Map показує .data168 B у SRAM; це статично перевірене виправлення, не нове вимірювання flash |
| Flush RX і pop byte/timestamp могли бачити різний стан після overflow/error | ISR може очищати ring, поки foreground читає його; перевірка одного volatile error counter до pop не утворює атомарний snapshot | Pop byte/timestamp/error epoch захищено PRIMASK; damaged stream відкидається до LF. Host test production serial.c перевіряє overflow, framing error на LF і відновлення. Реальний UART flood ще попереду |

Окремо статично виявлено ризик втрати SysTick ticks при довгому однобанковому flash stall. Guard uptime переведено на TIM3 accumulator, I²C timeout також на TIM3. Host-тест accumulator перевіряє 6000 с синтетичних кроків через uint32 microsecond wrap. Реальний flash stall не відтворено; це виправлення часової моделі, а не новий вимір.

Файли й точні версії пов'язані через source manifest та artifacts hashes. Жоден синтетичний cut-test, макет регістрів UART чи очікувана тривалість datasheet не видається за апаратний дослід.

Coverage мав відносні source paths, які не знаходилися з каталогу gcov, і старі archive .gcda з іншим timestamp. tests/Makefile тепер компілює coverage з абсолютними source paths і очищає обидва profile каталоги. Чистий `coverage-final-linux.log` не містить цих діагностик. Це виправлення host-інструментарію, не метрика MCU.

Parser раніше міг прийняти `arm` як C-string префікс рядка `arm<NUL>suffix`. Додано відхилення непечатних ASCII до LF; прямий grammar test і production UART fixture перевіряють NUL і0xFF, наступний цілий рядок приймається. Це host-доказ валідації, а не MCU flood.


## Негативний pilot1fe і новий polling-кандидат

Власний `lab3-pilot-1fe-20261028.vcd` триває 119.942155525 с, SHA256 c1ea6311e7b76a5119f2706cbf931936ac8840db3e1edadb700192dfd1b5a753. Два сусідні STATUS переходи мають MCU delta4295467 мс за VCD500.082325 і499.856850 мс. У тихому фрагменті2–40 с незалежний clock gate отримав offset span4294967.981775 мс,76 рядків, результат fail. Внутрішні STAT/load цього pilot недостовірні; це не CPU latency. Первинна периферійна причина не ізольована.

Erase повернувся до VERIFY_FAIL page1 offset0 word0x00000000 замість FF, SR=CR=0; failed1/ticket0. Код verify виконується після HAL_OK, реєстри зчитані після Lock. Успішний program/CRC/durability не підтверджено; загальна непідтримка моделі не встановлена. Деталі й raw у `docs/metrics/pilot-1fe-20261028-r2/`, шляхи від кореня repo; із цієї папки metrics це `pilot-1fe-20261028-r2/`.

Кандидат2c6940d3488ab256 прибирає UIF/IRQ high-word. Атомарне накопичення raw CNT виконується також із SysTick, щоб HAL startup wait не пропускав читання. Строга умова: інтервал читання<65536 мкс; її треба звірити незалежним VCD. 400000 host counter-кроків через16/32-bit wrap, негативний приклад uint32-backstep, macOS/Linux tests та sanitizers пройшли. Це не MCU перевірка. Діагностика Flash додає HAL status/error/CR і before/after readback. Повторний pilot і приймальні S1/S2/S3 лишаються потрібними.
