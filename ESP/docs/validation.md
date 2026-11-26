# Проверки и ресурсы

Дата текущего этапа: 26 ноября 2026. Windows host, ESP-IDF 6.1.1. Число физических плат не является
архитектурным лимитом. Ниже отдельно отмечаются portable tests, реальные TCP и физические устройства.

## Воспроизведение

Host suite:

```sh
cmake -S ESP/tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure --timeout 30
# Linux, отдельные каталоги для двух инструментов:
cmake -S ESP/tests/host -B build/asan -DRAVEN_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S ESP/tests/host -B build/tsan -DRAVEN_SANITIZER=thread -DCMAKE_BUILD_TYPE=Debug
```

Один CTest executable содержит 17 сценариев: golden codec/invalid input; Critical admission, coalescing
и tick под flood; fragment/coalesced framing и абсолютный timeout; handshake/keepalive/probe;
admission/Written/semantic deadline; response раньше Written; stale RX/TX после reuse; конкурентный
close против commit; потеря keepalive и ACK при полном TX; bounded journal; parser/reconnect;
send tickets, I/O leases и 200 переиспользований; шесть peers/duplicate identity; не более одного
optional diagnostic; SDK Log V1/V2 fragments; transport-independent PacketWriter; полный текст logs.

`RAVEN_TCP_BOARD_PROBE=y` в **node test build** заменяет обычную композицию тестовым клиентом.
`RAVEN_VALIDATION_MONITOR=y` добавляет UART snapshots каждые 10 секунд и дополнительную задачу
3072 bytes; default обеих опций — off. Это не сервисная команда и не часть production-протокола.

Для аппаратного TCP suite: обычные node временно останавливаются, MC работает с capacity=6, один
test node подключается к AP и создаёт шесть сессий с явно тестовыми IDs. Автоматически проверяются:
20 секунд handshake/keepalive всех peers; независимый close/reconnect; duplicate identity без takeover;
неподдерживаемая версия, плохая length, partial frame deadline; non-reading peer под нагрузкой;
последующее восстановление всех шести. Проверка здоровых сессий требует свежего keepalive (менее
3 секунд), а не только открытого fd и прежнего handshake. Отдельно повторяется с подключённой BLE-консолью.
После теста вернуть `RAVEN_TCP_BOARD_PROBE=n` и обычные node-прошивки.

UART capture не отправляет прикладные команды:

```powershell
python ESP/tests/capture_serial.py COM4 COM5 COM6 COM7 --seconds 1835 --output .cache/capture
# --reset COM5: обычный RTS/EN reset
# --rom-reset COM4: reset через ROM/esptool для native USB
python ESP/tests/check_capture.py .cache/capture --mc COM4 --nodes COM5 COM6 COM7 --seconds 1800
```

`check_capture.py` исключает первые 20 секунд восстановления, затем требует непрерывных snapshots,
трёх Ready bindings у MC и одного у каждого node, растущего last_seen, неизменных Boot ID/generation,
ресурсных порогов и отсутствия reset/watchdog/timeout. Для автоматической остановки/возврата плат есть
`--actions ESP/tests/four_boards.example.json`: номера COM в этом **стендовом сценарии** нужно изменить
под конкретное подключение. Он не задаёт список плат приложения. После последнего восстановления
стабильный интервал проверяется отдельно.

## Memory budget MC, capacity=6

Значения `sizeof` измерены компилятором Xtensa и подтверждены startup log на ESP32-S3.
Ссылки/очереди не выделяются динамически при каждом сообщении. Таблицы слотов принадлежат Runtime.

| Объект | Размер, bytes | Количество / вклад |
|---|---:|---:|
| Message | 88 | payload 64; раньше 48 с payload 24 |
| Inbox | 1 520 | 9 = 13 680; раньше 880 каждый |
| Dispatcher | 64 | 16 локальных endpoints; раньше 32 |
| Session | 136 | 6 = 816; включая два SendRecord по 48 |
| PeerSlot | 240 | 6 = 1 440 |
| PeerRecord | 72 | 12 = 864 |
| Journal Record | 128 | 64 = 8 192 |
| TcpEndpoint | 1 864 | 6 = 11 184; **включает** TX Inbox, не прибавлять его повторно |
| BLE transport | 4 200 | bounded RX×4/TX×8, control blocks и активный frame |
| Runtime целиком | 32 240 | всё выше + сервисы/владельцы/парсеры/ссылки; без task stacks |

Payload 64 определяется `Inbound{route, receive_time, Body}` и `Outbound{route, SendToken, Body}`:
оба ровно 64 bytes. Body 48 определяется Hello, а не произвольным будущим bulk-трафиком.
Размеры проверяются `static_assert`; изменение контракта требует нового расчёта.

| Application tasks | Stack bytes |
|---|---:|
| 6 TCP RX + 6 TCP TX | 36 864 |
| PeerService + DiagnosticsService | 8 192 |
| TCP manager + BLE RX + BLE TX | 9 216 |
| Итого: 17 задач | 54 272 |
| TCB, mutexes, RTOS metadata: резерв | 4 096 |

SDK tasks/radio pools идут сверх этих application stacks: Wi-Fi stack 6656, NimBLE host 4096,
lwIP task 3072 bytes, event/controller/idle/timer tasks и SDK globals учитываются в link map и
измерении heap после их запуска. NimBLE MSYS1/MSYS2/ACL pools настроены на 8 blocks каждый;
Wi-Fi static RX/dynamic RX/dynamic TX — по 6. PSRAM не используется.

Динамическая часть коммуникационного профиля, консервативные резервы поверх пустых сессий:

| Ресурс | Резерв, bytes | Основание |
|---|---:|---|
| TCP TX | 19 200 | 6×2 pbuf до 1600 bytes с MSS/header/metadata |
| TCP RX с L2→L3 copy | 20 736 | 6×2880 window + 36×96 metadata/header |
| Общие Wi-Fi dynamic RX | 12 288 | 6×2048 на выравнивание/полный frame |
| Общие Wi-Fi dynamic TX | 12 288 | 6×2048, Wi-Fi копирует TX |
| TCP/netconn/mailboxes/allocator | 6 144 | шесть bindings, очереди SDK и metadata |
| Connect/close transient reserve | 4 096 | параллельные lifecycle-переходы |
| Подключённый BLE-клиент | 6 144 | сверх startup radio pools; наблюдавшаяся разница около 4,7 KiB |
| Итого динамический резерв | 80 896 | не прибавлять Runtime/stack второй раз к уже измеренному heap |

В итоговой production-прошивке startup free internal DMA heap 110 664 bytes после радио и application owners.
После выхода из `app_main` освобождаются ещё 3584 bytes его стека (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`);
возвращаемый TCB в запас не засчитываем. Оценка production: 110 664 + 3584 − 80 896 = **33 352 bytes**.
Это консервативный резерв для данного ICD/window/profile, а не гарантия произвольной нагрузки всей
сети. Allocation overhead и фрагментация проверяются на железе по free/min, largest block и stack
high-water marks. Тестовая monitor task дополнительно расходует 3072 bytes стека и TCB; её расход
входит в аппаратные замеры ниже, но не в production-профиль.

Критерии стенда: минимум 32 KiB internal DMA heap, largest block ≥8 KiB, свободный стек каждого
application owner и NimBLE host ≥1 KiB. При увеличении capacity проверить эти критерии заново.

### Почему именно эти настройки lwIP

ESP-IDF 6.1.1 задаёт `LWIP_NETIF_TX_SINGLE_PBUF=1`; выключить TCP_OVERSIZE с ним нельзя — SDK не
собирается. `tcp_pbuf_prealloc` резервирует полный MSS даже под короткий кадр. Поэтому одного
`TCP_SND_BUF=2880` недостаточно для оценки множества маленьких отправок.

В `communication_profile.cmake`: `TCP_SND_QUEUELEN=4`, `TCP_SNDQUEUELOWAT=2`, `TCP_SNDLOWAT=128`.
Writer один и делает select перед каждым кадром ≤66 bytes. lwIP `api_msg.c` снимает writability
при двух queued pbuf; следующая запись происходит после освобождения места. Таким образом data pbuf
одного writer ограничены двумя. Копии TCP/Wi-Fi считаются отдельно. SDK sources не изменены.

Первый аппаратный slow-reader прогон воспроизвёл истощение общего RX pool на тестовом узле при
`L2_TO_L3_COPY=n`: другие его сессии потеряли heartbeat. После включения copy тот же тест проходит;
MC закрывает только slow stream с `send-deadline`. SDK `wlanif.c` подтверждает освобождение L2 buffer
после копирования, а retained L3 bytes учитываются в TCP RX budget выше.

## Зафиксированные результаты этапа

- Host: 17/17 сценариев проходят на Windows (Zig 0.15.2 / Clang 20.1.2, C++17, warnings as errors).
- ESP-IDF: MC ESP32-S3 и node ESP32 собраны. Реальный Wi-Fi TCP suite завершён всеми шестью PASS,
  без reset/panic/watchdog MC. После исправления L2 copy остальные пять peers переживают slow reader.
- MC с шестью TCP и BLE advertising: minimum internal DMA heap **93 568**, largest sampled block
  **65 536**, minimum sampled owner/host stack **1 252** bytes. Эти числа включают validation task.
- Пользователь подтвердил читаемые BLE `help/status/stats/logs/watch off` и оставил приложение подключённым.
- Повтор того же полного TCP suite с активным BLE завершён шестью PASS. MC: minimum heap **87 244**,
  minimum sampled largest block **63 488**, minimum sampled stack **1 252** bytes.
- Четыре физические платы первоначально подняли общий handshake: MC `E072A1D72014` (COM4), nodes
  `08A6F76BBA34` (COM5), `246F2836590C` (COM6), ESP32-CAM `D0EF765CC8BC` (после переноса USB — COM7).
  У CAM воспроизводилась потеря serial response, устранявшаяся снятием питания; это отдельное ограничение стенда.
- Проверены индивидуальные остановки каждого node на 15 секунд и возврат без нарушения остальных связей;
  остановка/возврат MC при работающих nodes; последовательные перезапуски COM5, COM7, COM6.
  Каждый вернулся с новым Boot ID, MC переиспользовал слоты с новым generation. Остановки делались
  удержанием приложения в ROM через esptool; физическое снятие питания CAM дополнительно выполнил пользователь.

- [CI для 3290f57](https://github.com/ilyxas/litu_raven/actions/runs/45737377589): все шесть jobs прошли —
  Linux host, ASan/UBSan, TSan; firmware MC ESP32-S3, node ESP32 и node ESP32-S3.

Итоговая непрерывная запись четырёх плат — 1835 секунд; после исключения первых 20 секунд
восстановления `check_capture.py --seconds 1800` прошёл на каждой плате (181–182 snapshots).
Все связи оставались Ready, last_seen рос, Boot ID/slot/generation не менялись; reset/watchdog,
таймаутов и новых diagnostic drops в этом интервале нет. На COM6 счётчик уже содержал одну потерю
диагностики при старте и далее не рос. BLE был подключён с 127-й секунды до конца записи.

| Плата | Minimum internal DMA heap | Minimum sampled largest block | Minimum owner/host stack |
|---|---:|---:|---:|
| COM4 / MC | 91 860 | 63 488 | 1 312 |
| COM5 / node | 198 192 | 110 592 | 1 480 |
| COM6 / node | 198 068 | 110 592 | 1 472 |
| COM7 / CAM node | 197 156 | 110 592 | 1 544 |

После выдержки выполнен полный повтор TCP suite с усиленной проверкой свежести keepalive и тем же
BLE-соединением, без перезапуска MC: **6/6 PASS**. Slow reader закрыт с `send-deadline`, остальные пять
сессий продолжили обмен; затем восстановлены все шесть. На этом прогретом MC с повторным использованием
сессий минимум heap **82 632**, largest sampled block **59 392**, minimum stack **1 256** bytes.
Reset/panic/watchdog MC и новых diagnostic drops нет. Здесь зафиксирован наименьший heap данного этапа.

На всех четырёх платах оставлены обычные прошивки с `RAVEN_VALIDATION_MONITOR=n` и
`RAVEN_TCP_BOARD_PROBE=n`: MC image 1 054 240 bytes, node image 826 640 bytes. Flash каждого устройства
проверена esptool по хешу; CAM прошита ROM loader с `--no-stub`. Последующее 60-секундное наблюдение
после reset MC подтвердило Ready всех трёх nodes и BLE_READY, отсутствие тестового вывода и новых
ошибок после восстановления связей. Serial-порты освобождены.

Старые отчёты «25/25 на Arm Demo» не являются проверкой этой реализации. Реального iOS-клиента и UART
transport в текущем этапе нет; их работоспособность не заявляется.
