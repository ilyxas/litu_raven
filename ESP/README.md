# Raven: обмен между устройствами

Общий runtime и два самостоятельных ESP-IDF приложения:

- `apps/mc` — ESP32-S3, Wi-Fi AP/TCP server и сервисная BLE-консоль `Raven-BLE`.
- `apps/node` — ESP32/ESP32-S3, Wi-Fi STA/TCP client. Камера пока не инициализируется.
- `components` — общее ядро, ICD, коммуникация, сервисы и платформенные адаптеры.
- `tests` — проверки контрактов на компьютере и тестовый клиент для реального MC.

Каждая плата имеет factory MAC как Device ID и случайный Boot ID. Список плат в коде отсутствует.
Новая обычная плата использует `node` с подходящим `IDF_TARGET`, теми же SSID/паролем и портом.
MC по умолчанию резервирует шесть одновременных сессий. Это ресурсный профиль сборки, а не число
обязательных устройств. При увеличении профиля пересчитываются RAM, сокеты, задачи и предел AP.

## Сборка и прошивка

Проверяемая версия SDK: ESP-IDF 6.1.1. Требуется стандартное экспортированное окружение IDF.

```sh
idf.py -C ESP/apps/mc -B build/mc -DIDF_TARGET=esp32s3 build
idf.py -C ESP/apps/node -B build/node -DIDF_TARGET=esp32 build
idf.py -C ESP/apps/mc -B build/mc -p PORT flash monitor
```

На этом Windows-компьютере можно использовать helper, пути SDK переопределяются переменными окружения:

```powershell
./ESP/tools/idf.ps1 -App mc -Target esp32s3 -IdfArguments build
./ESP/tools/idf.ps1 -App node -Target esp32 -IdfArguments build
./ESP/tools/idf.ps1 -App node -Target esp32 -IdfArguments @('-p','COM7','flash')
```

`menuconfig → Raven device profile` содержит настройки роли, ёмкости, сети и тестовых режимов.
Раздел приложения укладывается в 2 MiB flash; PSRAM не требуется. MC использует native USB console,
обычный ESP32 node — UART. Для node на ESP32-S3 при необходимости выбери USB Serial/JTAG console.
По умолчанию тестовые режимы выключены.

Сеть разработки: `Raven-Link`, пароль `raven-link`, TCP `8080`. Это открытый доверенный стенд:
Device ID обнаруживает конфликты, но не аутентифицирует устройство. Защита продуктовой сети — отдельный контракт.

## Проверки и документация

```sh
cmake -S ESP/tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

- [Архитектурная спецификация Raven](docs/architecture.md).
- [Неизменяемый архитектурный baseline из 02dd622](docs/architecture-baseline-02dd622.md).
- [Проверка полноты спецификации относительно baseline](docs/architecture-coverage.md).
- [Коммуникационная реализация и lifecycle](docs/communication-architecture.md).
- [ICD, framing и временная семантика](docs/communication.md).
- [BLE-консоль](docs/ble-console.md).
- [Ресурсы и воспроизводимые проверки](docs/validation.md).
- [ADR-003: границы и сознательная очистка](docs/adr/0003-device-communication.md).

Arm Demo, FC/MSP, старые TCP/JSON/telemetry gateways и совместимость ESP с прежним iOS-протоколом
удалены. iOS-проект не менялся. История предыдущих экспериментов доступна в Git и ADR-001/002;
они не описывают текущую прошивку.
