# Результати ЛР2 Lockbox

Омельченко Богдан Юрійович, ІО-32. Одна прошивка STM32C031C6 одночасно читає MPU6050, LDR і PIR, калібрує охорону, незалежно фіксує SHOCK та LIGHT, обробляє кнопку MODE й веде UART/RAM-журнал.

## Перевірені сценарії

Основний власний запис `guard-rx-interrupt-full.vcd` підтвердив калібрування за 498 ms, SHOCK із cause=1, додавання LIGHT із cause=3, навмисний NACK, IMU_FAULT/IMU_RECOVERED, PIR і зняття охорони кнопкою. Журнал містив 9 подій, dropped=0, rx_errors=0. Повний запис збережено в сусідньому `Lab02/Work/captures`; SHA-256 та розмір наведено в [manifest.json](captures/manifest.json).

Окремий прогін `pir-only-armed-full.vcd` підтвердив MOTION_ON та MOTION_OFF у режимі ARMED із cause=0. Прогін `light-first-full.vcd` підтвердив LIGHT із cause=2 та наступний SHOCK із cause=3. Фрагменти й декодовані події доступні як `pir-only-armed.*`, `light-only-armed.*`, `shock-after-light.*` у `docs/captures`. Дані не замінювалися очікуваними значеннями.

Виміряно SCL 100 kHz, UART bit time 8.6875 µs, медіанний період ADC 20.002325 ms та ширину ADC_MARK 74.850 µs. Зняття ALARM настало через 33.000950 ms після останнього дребезгу MODE. Метод, часові межі та аномалії описано в [аналізі записів](captures/analysis.md).

## Тести та складання

10 груп тестів пройшли на Linux/GCC і macOS/Clang; AddressSanitizer та UndefinedBehaviorSanitizer не повідомили помилок. Переносимі analog, MPU6050, commands і Guard мають 100% покриття рядків. Для Guard GCC виконав 100% гілок, взято 97.37% напрямів. Core, Board, HAL та IRQ не входять до host coverage. Логи й межі перевірки: [docs/validation](validation/README.md).

ARM GCC 10.3.1 зібрав прошивку: text=18512, data=120, bss=6888; Flash=18632 B, RAM=7008 B. Власний C компілюється з `-Wall -Wextra -Werror`. Docker використовує зафіксований digest Ubuntu 22.04, але apt archive не є snapshot, тому майбутню байтову ідентичність не гарантовано. Workflow GitHub Actions для Ubuntu 24.04 виконує build, test, sanitize та coverage; результат віддаленого запуску слід перевіряти окремо.

## Документи й відтворення

У сусідньому `Lab02/Reports` збережено фінальний PDF на 16 сторінках. Звіт продовжує шаблон ЛР1 зі збереженими титулом, стилями та полями. Скриншоти PulseView переглянуто у повному розмірі: на них немає вказівника або підсвічування Codex.

PNG/SVG і `.dot` містять схеми компонентів та станів. `tools/plot_captures.py` будує графіки лише з власного запису з перевіркою SHA-256; `tools/monitor.py` читає RFC2217 UART у VS Code.

На сайті Wokwi використовувати `Upload Firmware and Start Simulation…` із власним `firmware/build/lockbox.hex`. Після Stop повторно завантажити HEX: звичайні Start або Restart можуть зібрати вихідний Arduino sketch відкритого шаблону.

## Невирішені питання

CPU/UART повертає WHO_AM_I=68, тоді як незалежні декодери SDA дають EB. Розходження є і для шести байтів прискорення. HAL read зупинявся після ACK, власний адаптер відновив функціональні сценарії, але фізичну правильність читання не доведено. Причину не встановлено. Фізичного стенда, вимірів струму й шуму немає; STM32C031C6 треба погодити з викладачем замість рекомендованих плат.

ЛР1 зведено локально в main на 9d6190a. Для ЛР2 передбачено рівно 10 змістовних комітів у `feature/lab-2` і тег `lab2`. Віддалений main залишено без змін. PR створює користувач: поки remote main не містить ЛР1, diff цієї гілки до нього включатиме також ЛР1.
