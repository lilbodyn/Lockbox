# Lockbox, ЛР3: попередні результати

> Історичний матеріал попередніх кандидатів. Актуальний source `484fae964f8e7cb1` і результати S1/S2 описані в [стані ЛР3](lab3-status-20261101.md). Старі твердження про поточний TIM3 і відсутність години не стосуються нового запуску.

Омельченко Богдан Юрійович, ІО-32. Робота продовжує власну модель шафки: калібрування перед ARMED, SHOCK/LIGHT з фіксацією причин до disarm, PIR лише як спостереження. STM32C031C6 збережено, RTOS не використовується.

Кандидат `2c6940d3488ab256` має phase-locked releases, TIM3 1 MHz, накопичення uptime Guard через microsecond wrap, EXTI timestamps, bounded UART, поступовий export і двосторінковий CRC archive. Усі точні хеші в `metrics/source-manifest.json` та `artifacts.json`. Guard/Archive не залежать від HAL. Переліки задач, меж і сценаріїв містяться в [tasks](tasks.md), [limitations](limitations.md), [baseline](baseline-lab4.md).

ARM build, macOS/Clang та Linux/GCC host tests, ASan/UBSan пройшли. CRC/recovery перевірено на 2178 byte-cut випадках та 512 single-bit пошкодженнях. Окремий host-тест перевіряє production UART code через підставні регістри. 3600000 синтетичних releases і 6000 с accumulator ticks не є годиною MCU.

Власний Wokwi VCD попередньої версії `14ab26d1a2a4730a` триває 115.749970625 с, містить 5782 SAMPLE імпульси. Міжстартовий період min/avg/max 6316.825 / 19999.989 / 33721.275 мкс. Довгий інтервал співпав зі stats; handler latency становить 28808 мкс. Перші 422 задачі за TIM3 мають avg 1149 мкс, за GPIO width avg 1107.133 мкс; обгортка включає додаткові точки обліку. Дані, сценарій і межі порівняння описано в [metrics](metrics/README.md).

У цьому запуску перша спроба erase дала failed=1/ticket=0. Постійний носій не підтверджено. Кандидат додає поділ unlock/HAL/verify діагностики; підготовлено окремий factory-erased HEX, який ще не запускали. Жодної HAL_Delay-імітації flash немає.

Пам'ять кандидата: flash 26896 B у програмному регіоні 28672 B, окремий archive reserve 4096 B; SRAM .data168 B + .bss7364 B, heap 512 B, stack 2048 B, alignment 4 B; разом 10096/12288 B, запас 2192 B. Повний статичний stack bound для prebuilt libc не заявлено. Старий paint 968/2048 B і heap 0 не є динамічними числами кандидата.

Повна ЛР3 ще не прийнята. Не виконано нові S1/S2/S3 кандидата, годину S2, 30 фізичних подій під навантаженням, UART flood, реальний breaking point, startup-not-ready, успішний archive/reset і фізичний power-loss дослід. Числові FR не позначаються прийнятими без максимуму S3. Немає відео, створеного за цей етап.

Проблеми та виправлення наведено в [lab3-problems](lab3-problems.md). ЛР4 лишається набором гіпотез і методикою повторення, її код не додано.


## Негативний pilot1fe і новий polling-кандидат

Власний `lab3-pilot-1fe-20261028.vcd` триває 119.942155525 с, SHA256 c1ea6311e7b76a5119f2706cbf931936ac8840db3e1edadb700192dfd1b5a753. Два сусідні STATUS переходи мають MCU delta4295467 мс за VCD500.082325 і499.856850 мс. У тихому фрагменті2–40 с незалежний clock gate отримав offset span4294967.981775 мс,76 рядків, результат fail. Внутрішні STAT/load цього pilot недостовірні; це не CPU latency. Первинна периферійна причина не ізольована.

Erase повернувся до VERIFY_FAIL page1 offset0 word0x00000000 замість FF, SR=CR=0; failed1/ticket0. Код verify виконується після HAL_OK, реєстри зчитані після Lock. Успішний program/CRC/durability не підтверджено; загальна непідтримка моделі не встановлена. Деталі й raw у `docs/metrics/pilot-1fe-20261028-r2/`, шляхи від кореня repo; із цієї папки metrics це `pilot-1fe-20261028-r2/`.

Кандидат2c6940d3488ab256 прибирає UIF/IRQ high-word. Атомарне накопичення raw CNT виконується також із SysTick, щоб HAL startup wait не пропускав читання. Строга умова: інтервал читання<65536 мкс; її треба звірити незалежним VCD. 400000 host counter-кроків через16/32-bit wrap, негативний приклад uint32-backstep, macOS/Linux tests та sanitizers пройшли. Це не MCU перевірка. Діагностика Flash додає HAL status/error/CR і before/after readback. Повторний pilot і приймальні S1/S2/S3 лишаються потрібними.
