# Межі перевірки

ARM build виконується macOS-host → Docker Ubuntu22.04 → ARM GCC10.3.1. Linux tests у тому самому контейнері на ARM64. Другий host Clang/macOS ARM64. GitHub CI конфігурація Ubuntu24.04; до запуску workflow на remote не стверджуємо його успіх.

Core і HAL, доступ до реєстрів I2C/ADC/UART, SysTick та interrupt ring не охоплені host coverage. Вони перевірені Wokwi на функціональність, а I2C має задокументоване невирішене розходження wire read/CPU data. Фізична перевірка не проводилась.

Assert.h зупиняє suite на першому failure і не має іменованих parametrized cases чи окремого XML runner. Recording fake зберігає register map, перевіряє адресу, розмір та порядок записів. Tests вручну задають errors на всіх operation positions; це не перевірка електричних front/ACK на платі.

HAL1.0.1 взято з офіційного commit24999b605d0b9da3e4675c4c884036ae724bb59f. Початкове vendoring inherited CMSIS з ЛР1 збережено. Docker base digest фіксований, але apt package archives не snapshot: абсолютна byte reproducibility через рік не гарантована. Версія ARM GCC зафіксована фактичним environment log.
