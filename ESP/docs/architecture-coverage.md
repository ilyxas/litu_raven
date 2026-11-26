# Проверка полноты архитектурной спецификации

## Область проверки

Сопоставлены неизменяемый [baseline](architecture-baseline-02dd622.md) и
[нормативная спецификация Raven](architecture.md). Проверка относится к смысловой
полноте документа: аксиомам, определениям, обязанностям, запретам, инвариантам и
взаимодействиям. Это не Architecture Compliance Review реализации PR.
Здесь не утверждается соответствие текущего кода новым или сохранённым требованиям.

Техническое описание конкретного обмена находится в
[communication-architecture.md](communication-architecture.md).
Оно не является заменой спецификации и не использовалось как основание ослабить
архитектурное правило. Handoff не является источником нормативных положений.

## Неизменяемый источник

- Коммит: `02dd622f5e2e08fa8f0463dcef9ae21358191f7b`.
- Исходный путь: `ESP/raven/docs/architecture.md`.
- Копия: `ESP/docs/architecture-baseline-02dd622.md`.
- Git blob исходника и копии: `f9aa0dd686e92f764253727abd8ea9fdf049f1cd`.
- SHA-256: `c2afd8f1ebd9e594b4caa3c2f09b7cc4a5b4e43f70334efe5365bc171bd0faa4`.
- Размер: **16 356 bytes**, **341 строка**, исходные LF.
- Копия зафиксирована отдельно в `fbdb7ac`; преобразование строк для неё отключено
  через локальный `.gitattributes`.

Baseline нельзя редактировать, сокращать, форматировать, переводить или
актуализировать. Даже исторические ссылки остаются побайтно исходными;
они не являются инструкциями запуска текущего приложения. Метаданные происхождения
и сопоставление хранятся в этом файле, не добавляются в baseline.

Воспроизводимая побайтная проверка из корня репозитория:

```python
from pathlib import Path
import hashlib
import subprocess

source = subprocess.check_output([
    "git", "show", "02dd622:ESP/raven/docs/architecture.md"
])
copy = Path("ESP/docs/architecture-baseline-02dd622.md").read_bytes()
assert copy == source
assert hashlib.sha256(copy).hexdigest() == (
    "c2afd8f1ebd9e594b4caa3c2f09b7cc4a5b4e43f70334efe5365bc171bd0faa4"
)
assert len(copy) == 16356 and len(copy.splitlines()) == 341
print("Baseline: byte-identical")
```

## Как читать сопоставление

Номера строк всегда относятся к baseline. Для строки с несколькими самостоятельными
утверждениями даны отдельные записи. Перечисленные источники, функции и запреты
также проверены отдельно; повторение нормы в инвариантах или схеме не пропущено.

- **С** — архитектурный смысл сохранён.
- **М** — сохранён смысл, изменено описание конкретного механизма/backend.
- **К** — исторический контекст или предметный пример; архитектурное свойство
  явно присутствует в указанном месте спецификации.
- **У** — устранена двусмысленность или неточность пояснения без ослабления нормы.

Заголовки, пустые строки, ограждения схем и горизонтальные разделители не задают
дополнительных требований; их тематическая принадлежность отражена диапазонами.
Обозначения ACT/SVC/STATE/ROOT/DEL с номером указывают конкретный пункт
соответствующего раздела. A01–A14 и INV-01–INV-07 сохраняют исходную нумерацию.

## Вступление и интерпретация

| ID | Строки baseline | Содержательное положение | Где сохранено | Итог |
|---|---|---|---|---|
| P01 | 1 | Документ описывает архитектуру Raven целиком. | [Назначение](architecture.md#scope) | С: общая нормативная область. |
| P02 | 3-5 | Delivery/execution-контракты имеют отдельное описание, связанное с общей моделью. | [Admission и исполнение](architecture.md#admission-execution), [коммуникация](architecture.md#communication-extension) | К: ссылки на прототип остаются в baseline; действующие общие контракты изложены самостоятельно. |
| P03 | 5 | Ролевая модель продолжает действовать. | [Назначение](architecture.md#scope), [A02](architecture.md#a02) | С: роли не зависят от состава экземпляров. |
| P04 | 6 | FC-композиция доступна как альтернативный boot mode. | [A14](architecture.md#a14), [адаптеры](architecture.md#role-adapter) | К: факт конкретной композиции не превращён в общую обязанность поддерживать FC. |
| P05 | 8 | Концептуальная модель, границы ролей и инварианты обязательны для участников. | [Назначение](architecture.md#scope), [инварианты](architecture.md#invariants) | С. |
| P06 | 8 | Архитектура направляет чтение кода при временных shortcuts и экспериментах. | [Назначение](architecture.md#scope), [порядок решений](architecture.md#governance) | С: отклонение кода не переопределяет норму. |
| P07 | 14 | Navigation, synthetic telemetry, joystick и manual mode — архитектурные probes. | [A14](architecture.md#a14) | К: предметные названия не ограничивают общий смысл probes. |
| P08 | 14 | Probes проверяют responsiveness, stress, ownership, routing, message/tick interaction. | [A14](architecture.md#a14), [исполнение](architecture.md#owner-execution) | С: все пять свойств перечислены. |
| P09 | 14 | Удаление примеров сохраняет проверявшиеся роли и контракты. | [A14](architecture.md#a14) | С. |
| P10 | 14 | Первый физический срез — Betaflight UART/MSP. | [A04](architecture.md#a04), [A13](architecture.md#a13), [связи ролей](architecture.md#role-interactions) | К: частный срез, а не определение Raven. |
| P11 | 16-18 | Направленность ветки отличается от расширения прежнего demo. | [Назначение](architecture.md#scope), [A14](architecture.md#a14) | К: этап/ветка не задают архитектурную модель. |
| P12 | 18 | Переиспользуемые механизмы сохраняют смысл при отсутствии текущего потребителя. | [A06](architecture.md#a06), [A07](architecture.md#a07), [A14](architecture.md#a14) | С: потребность в конкретном классе отделена от существования возможности. |
| P13 | 18 | Отсутствие конкретной Activity не отменяет Activity. | [A02](architecture.md#a02), [Activity](architecture.md#role-activity), [A14](architecture.md#a14) | С: имя BaseActivity не является условием существования роли. |
| P14 | 18 | TCP gateways остаются допустимыми адаптерами при UART/MSP-срезе. | [A03](architecture.md#a03), [A04](architecture.md#a04), [адаптеры](architecture.md#role-adapter) | С/М: транспортная заменяемость сохранена без требования конкретного gateway-класса. |
| P15 | 20-28 | UART adapter → framing/gateway → semantic client → Service owner → State/events. | [A04](architecture.md#a04), [Service](architecture.md#role-service), [связи ролей](architecture.md#role-interactions) | К/М: все роли переходов сохранены; предметный протокол не навязан архитектуре. |
| P16 | 30 | Любой слой допускает ревизию; hardware tests выявляют проблемы исполнения, владения и адаптеров. | [Порядок решений](architecture.md#governance), [A14](architecture.md#a14) | С: ревизия явная, тест не меняет норму автоматически. |

## Все 14 аксиом

| ID | Строки baseline | Содержательное положение | Где сохранено | Итог |
|---|---|---|---|---|
| A01.1 | 32-36 | Raven — platform-independent behavioural system; ESP/FreeRTOS — backend. | [A01](architecture.md#a01) | С. |
| A01.2 | 37 | Роли и контракты действуют в simulation. | [A01](architecture.md#a01), [A13](architecture.md#a13) | С. |
| A01.3 | 38 | Desktop execution допустим. | [A01](architecture.md#a01) | С. |
| A01.4 | 39 | Hardware необязателен на архитектурном уровне. | [A01](architecture.md#a01) | С. |
| A01.5 | 40 | Platform-specific детали ниже behavioural core. | [A01](architecture.md#a01), [зависимости](architecture.md#dependencies) | С. |
| A02.1 | 42-44 | Activity, Service, State выражают поведение и не являются ESP-конструкциями. | [A02](architecture.md#a02) | С. |
| A02.2 | 45 | Activity владеет orchestration и mode logic. | [A02](architecture.md#a02), [Activity](architecture.md#role-activity) | С. |
| A02.3 | 46 | Service владеет subsystem work. | [A02](architecture.md#a02), [Service](architecture.md#role-service) | С. |
| A02.4 | 47 | State владеет canonical truth. | [A02](architecture.md#a02), [State](architecture.md#role-state) | С. |
| A02.5 | 49 | Платформенная зависимость внутри поведения — leakage, не intended design. | [A02](architecture.md#a02) | С: несоответствие требует явного решения, не легализуется наличием кода. |
| A03.1 | 51-53 | Происхождение сообщения не определяет его предметный смысл. | [A03](architecture.md#a03) | С. |
| A03.2 | 54 | TCP — допустимый источник. | [A03](architecture.md#a03) | С. |
| A03.3 | 55 | RF control — допустимый источник. | [A03](architecture.md#a03) | С. |
| A03.4 | 56 | Mock input — допустимый источник. | [A03](architecture.md#a03) | С. |
| A03.5 | 57 | Simulation — допустимый источник. | [A03](architecture.md#a03) | С. |
| A03.6 | 58 | Synthetic sensors — допустимый источник. | [A03](architecture.md#a03) | С. |
| A03.7 | 59 | Локальная генерация — допустимый источник. | [A03](architecture.md#a03) | С. |
| A03.8 | 61-62 | Значим semantic meaning. | [A03](architecture.md#a03) | С. |
| A03.9 | 63 | Значим payload contract. | [A03](architecture.md#a03), [A05](architecture.md#a05) | С. |
| A03.10 | 64 | Значима legitimacy of route. | [A03](architecture.md#a03), [A05](architecture.md#a05) | С. |
| A03.11 | 65 | Значим handling owner. | [A03](architecture.md#a03), [A09](architecture.md#a09) | С. |
| A03.12 | 67 | Компонент реагирует на контракт, а не transport path. | [A03](architecture.md#a03) | С: семантическая identity не подменяется знанием транспорта. |
| A04.1 | 69-71 | Wire formats, framing, msg_id, codecs, gateways — граница. | [A04](architecture.md#a04) | С/М: функции сохранены без обязательного имени класса. |
| A04.2 | 72 | Принять внешнее представление. | [A04](architecture.md#a04), [ADP-01](architecture.md#role-adapter) | С. |
| A04.3 | 73 | Проверить представление. | [A04](architecture.md#a04), [ADP-01](architecture.md#role-adapter) | С. |
| A04.4 | 74 | Перевести в внутренний semantic contract. | [A04](architecture.md#a04), [ADP-01](architecture.md#role-adapter) | С. |
| A04.5 | 75 | Доставить надлежащему владельцу. | [A04](architecture.md#a04), [A05](architecture.md#a05) | С. |
| A04.6 | 77 | Transport origin не становится behavioural meaning. | [A03](architecture.md#a03), [A04](architecture.md#a04) | С. |
| A05.1 | 79-83 | Routing — strict admission, а не generic packet forwarding. | [A05](architecture.md#a05) | С. |
| A05.2 | 84 | Сообщение известно. | [A05](architecture.md#a05) | С. |
| A05.3 | 85 | Payload соответствует контракту. | [A05](architecture.md#a05), [DEL-07](architecture.md#role-delivery) | С: размер не объявлен полной семантической проверкой. |
| A05.4 | 86 | Существует decoder/adapter. | [A05](architecture.md#a05) | М/У: внешний вход декодируется; готовое внутреннее сообщение не требует повторного wire decode. |
| A05.5 | 87 | Маршрут явно зарегистрирован. | [A05](architecture.md#a05), [ROOT-05](architecture.md#role-coordinator) | С. |
| A05.6 | 88 | Получатель предназначен для сообщения. | [A05](architecture.md#a05), [DEL-07](architecture.md#role-delivery) | С. |
| A05.7 | 90 | Неподготовленный компонент не получает данные. | [A05](architecture.md#a05) | С. |
| A06.1 | 92-94 | Роли определяются ответственностью, не RTOS-примитивами. | [A06](architecture.md#a06) | С. |
| A06.2 | 96 | Activity — orchestration role. | [A06](architecture.md#a06) | С. |
| A06.3 | 97 | Service — subsystem execution role. | [A06](architecture.md#a06) | С. |
| A06.4 | 99 | Task/queue/tick/scheduling ниже ролей; domain не мыслит FreeRTOS-терминами. | [A06](architecture.md#a06), [backend](architecture.md#role-platform) | С. |
| A07.1 | 101-103 | Исполнительный механизм размещает роли, не определяет их. | [A07](architecture.md#a07) | С/М: BaseTask не является архитектурной обязанностью. |
| A07.2 | 104 | Асинхронное исполнение. | [A07](architecture.md#a07), [owner execution](architecture.md#owner-execution) | С. |
| A07.3 | 105 | Queue-driven delivery. | [A07](architecture.md#a07), [DEL-02](architecture.md#role-delivery) | С. |
| A07.4 | 106 | Periodic ticks. | [A07](architecture.md#a07), [A12](architecture.md#a12) | С. |
| A07.5 | 107 | Payload lifetime handling. | [A07](architecture.md#a07), [A08](architecture.md#a08) | С. |
| A07.6 | 108 | Разделение component contexts. | [A07](architecture.md#a07), [A09](architecture.md#a09) | С: не заявлена изоляция адресных пространств. |
| A07.7 | 110 | Activity/Service выражаются выше task machinery. | [A06](architecture.md#a06), [A07](architecture.md#a07) | М: обязанности BaseActivity/BaseService отделены от исторического наследования. |
| A08.1 | 112-114 | Самодостаточность при enqueue; queue не выполняет произвольный deep copy автоматически. | [A08](architecture.md#a08) | С/М: обязательное владение явно, способ копирования не навязан. |
| A08.2 | 116-117 | Независимость от lifetime sender buffer. | [A08](architecture.md#a08) | С. |
| A08.3 | 118 | Payload действителен до конца handling. | [A08](architecture.md#a08) | С. |
| A08.4 | 119 | Межзадачная доставка memory-safe по контракту. | [A08](architecture.md#a08) | С: копирование указателя недостаточно. |
| A09.1 | 121-123 | Meaningful work в контексте владельца. | [A09](architecture.md#a09) | С. |
| A09.2 | 124 | Не в event callback. | [A09](architecture.md#a09), [DEL-06](architecture.md#role-delivery) | С. |
| A09.3 | 125 | Не в transport ingress. | [A09](architecture.md#a09), [COM-02](architecture.md#communication-extension) | С. |
| A09.4 | 126 | Не в foreign task. | [A09](architecture.md#a09) | С. |
| A09.5 | 128 | Много источников не меняют владельца decision/subsystem work. | [A09](architecture.md#a09), [owner execution](architecture.md#owner-execution) | С. |
| A10.1 | 130-132 | Данные нескольких компонентов принадлежат State, не разрозненным локальным деталям. | [A10](architecture.md#a10), [SVC-08](architecture.md#role-service) | С. |
| A10.2 | 134 | Activity/Service читают State и обновляют через controlled ownership paths. | [A10](architecture.md#a10), [ACT-07](architecture.md#role-activity), [STATE-01/02](architecture.md#role-state) | С. |
| A10.3 | 134 | State — canonical shared system knowledge. | [A10](architecture.md#a10), [State](architecture.md#role-state) | С. |
| A11.1 | 136-140 | Activity определяет, что делать системе. | [A11](architecture.md#a11), [ACT-02](architecture.md#role-activity) | С. |
| A11.2 | 141 | Service исполняет subsystem work. | [A11](architecture.md#a11) | С. |
| A11.3 | 142 | State хранит system truth. | [A11](architecture.md#a11) | С. |
| A11.4 | 143 | Adapter соединяет core с внешним миром. | [A11](architecture.md#a11), [адаптеры](architecture.md#role-adapter) | С. |
| A11.5 | 145 | Activity не выполняет low-level subsystem work. | [A11](architecture.md#a11), [ACT-06/08](architecture.md#role-activity) | С. |
| A11.6 | 145 | Service не принимает system-wide orchestration decisions. | [A11](architecture.md#a11), [SVC-06](architecture.md#role-service) | С. |
| A12.1 | 147-149 | Tick — first-class execution model, не workaround. | [A12](architecture.md#a12) | С. |
| A12.2 | 151 | Activity может менять policy на своём tick при согласованной работе runtime. | [A12](architecture.md#a12) | С. |
| A12.3 | 152 | Message-driven behaviour. | [A12](architecture.md#a12) | С. |
| A12.4 | 153 | Time-driven behaviour. | [A12](architecture.md#a12) | С. |
| A12.5 | 155 | Policy loops, supervision, autonomy, sensor-driven behaviour. | [A12](architecture.md#a12) | С: все четыре назначения сохранены. |
| A13.1 | 157-159 | Hardware — не единственный provider. | [A13](architecture.md#a13) | С. |
| A13.2 | 160 | Физические sensors. | [A13](architecture.md#a13) | С. |
| A13.3 | 161 | Simulated sensors. | [A13](architecture.md#a13) | С. |
| A13.4 | 162 | Mock devices. | [A13](architecture.md#a13) | С. |
| A13.5 | 163 | Synthetic control streams. | [A13](architecture.md#a13) | С. |
| A13.6 | 164 | Replayed data. | [A13](architecture.md#a13) | С. |
| A13.7 | 166 | Behavioural core не меняется с provider; реальность источника известна нижнему слою. | [A13](architecture.md#a13), [PLAT-03](architecture.md#role-platform) | С. |
| A14.1 | 168-170 | Конкретные подсистемы проверяют, не определяют архитектуру. | [A14](architecture.md#a14) | С. |
| A14.2 | 171 | Проверка responsiveness. | [A14](architecture.md#a14) | С. |
| A14.3 | 172 | Проверка stress handling. | [A14](architecture.md#a14) | С. |
| A14.4 | 173 | Проверка message routing. | [A14](architecture.md#a14) | С. |
| A14.5 | 174 | Проверка ownership boundaries. | [A14](architecture.md#a14) | С. |
| A14.6 | 175 | Проверка orchestration/execution interaction. | [A14](architecture.md#a14) | С. |
| A14.7 | 177 | Probe можно удалить; его состав не граница Raven. | [A14](architecture.md#a14) | С. |
| A14.8 | 177 | Архитектурные выводы продолжают проверяться real hardware. | [A14](architecture.md#a14), [порядок решений](architecture.md#governance) | С. |
| A14.9 | 179 | Аксиомы направляют интерпретацию репозитория; слои выражают устойчивые роли. | [Конец раздела аксиом](architecture.md#a14), [назначение](architecture.md#scope) | С. |

## Определения, ответственности и ограничения ролей

| ID | Строки baseline | Содержательное положение | Где сохранено | Итог |
|---|---|---|---|---|
| ACT.D1 | 185-187 | Activity — high-level orchestration/FSM-like layer. | [Activity, определение](architecture.md#role-activity) | С. |
| ACT.D2 | 188 | Режимы boot/idle/manual/autonomous/recovery/fault и владение переходами. | [Activity, определение](architecture.md#role-activity) | С/К: перечисление сохранено как пример, не ограничение vehicle. |
| ACT.R1 | 190-192 | Подписка на system events и реакция на State. | [ACT-01](architecture.md#role-activity) | С. |
| ACT.R2 | 193 | Выбор нужных режиму сервисов. | [ACT-02](architecture.md#role-activity) | С: активность возможностей не приравнена к созданию RTOS-задачи. |
| ACT.R3 | 194 | Directed commands при mode transition. | [ACT-03](architecture.md#role-activity) | С. |
| ACT.R4 | 195 | Shared state snapshots для orchestration decisions. | [ACT-04](architecture.md#role-activity) | С. |
| ACT.R5 | 196 | Своя FreeRTOS task и inbound queue; решения исполняются там. | [ACT-05](architecture.md#role-activity), [A06](architecture.md#a06), [INV-06](architecture.md#invariants) | М: owner context и вход обязательны; FreeRTOS — backend. |
| ACT.N1 | 198-200 | Запрет прямого доступа к hardware/peripheral API. | [ACT-06](architecture.md#role-activity) | С. |
| ACT.N2 | 201 | Запрет прямой мутации shared state, запись через Controller. | [ACT-07](architecture.md#role-activity) | С/У: согласовано с 134; публикация собственного состояния через State API разрешена, обход — нет. |
| ACT.N3 | 202 | Запрет blocking I/O. | [ACT-08](architecture.md#role-activity) | С: не ослаблен до допуска произвольного bounded I/O в Activity. |
| ACT.N4 | 203 | Запрет business logic в ESP-IDF callback. | [ACT-09](architecture.md#role-activity) | С/М: распространяется на платформенный callback любого backend. |
| SVC.D1 | 207-209 | Service — asynchronous subsystem worker для ongoing/reactive work. | [Service, определение](architecture.md#role-service), [SVC-01](architecture.md#role-service) | С. |
| SVC.D2 | 211 | Инкапсуляция hardware subsystem либо software stack; мотор/датчики/Wi-Fi/telemetry/light/audio — примеры. | [Service, определение](architecture.md#role-service), [A14](architecture.md#a14) | С/К: предметный перечень не ограничивает связную подсистемную ответственность. |
| SVC.R1 | 213-215 | Постоянное обслуживание в собственной FreeRTOS task. | [SVC-01](architecture.md#role-service), [A06](architecture.md#a06), [INV-06](architecture.md#invariants) | М: постоянный owner lifecycle без требования busy loop и конкретной RTOS. |
| SVC.R2 | 216 | Чтение hardware/external stacks и запись общего знания в State. | [SVC-02](architecture.md#role-service) | С. |
| SVC.R3 | 217 | Broadcast событий о значимых фактах/переходах. | [SVC-03](architecture.md#role-service), [DEL-01](architecture.md#role-delivery) | С: Journal не объявлен эквивалентом поведенческой публикации. |
| SVC.R4 | 218 | Получение и исполнение directed commands из inbound queue. | [SVC-04](architecture.md#role-service) | С. |
| SVC.N1 | 220-222 | Запрет system-wide mode decisions. | [SVC-06](architecture.md#role-service) | С. |
| SVC.N2 | 223 | Запрет доступа к чужим Service internals. | [SVC-07](architecture.md#role-service), [зависимости](architecture.md#dependencies) | С. |
| SVC.N3 | 224 | Общие mutable-данные не разбрасываются по Service fields, принадлежат Controller. | [SVC-08](architecture.md#role-service), [STATE-04](architecture.md#role-state) | С: публичный метод конкретного Service не подменяет самостоятельный State-контракт. |
| STATE.D1 | 228-230 | Thread-safe canonical stores, SSOT общего знания. | [State, определение](architecture.md#role-state) | С. |
| STATE.D2 | 232 | Единственный авторитетный ответ о battery/Wi-Fi/pose, не разрозненные Service fields. | [State, определение](architecture.md#role-state), [A10](architecture.md#a10) | С: три примера сохранены. |
| STATE.R1 | 234-236 | Thread-safe write API сервисам. | [STATE-01](architecture.md#role-state) | С: разрешённые владельцы записи объявляются явно. |
| STATE.R2 | 237 | Thread-safe snapshots/read API Activity и Service. | [STATE-02](architecture.md#role-state) | С. |
| STATE.R3 | 238 | Mutex/atomics защищают внутренние данные. | [STATE-03](architecture.md#role-state) | С/М: эквивалентные средства синхронизации допустимы. |
| STATE.N1 | 240-242 | Нет business logic. | [STATE-05](architecture.md#role-state) | С: проверки целостности хранилища не дают права выбирать предметное поведение. |
| STATE.N2 | 243 | Нет events/commands. | [STATE-06](architecture.md#role-state) | С. |
| STATE.N3 | 244 | Нет FreeRTOS task. | [STATE-07](architecture.md#role-state), [INV-06](architecture.md#invariants) | С/М: нет автономного исполнителя при любом backend. |
| ROOT.D1 | 248-250 | Единственный root composition/bootstrap, построение и wiring объектов. | [Coordinator, определение](architecture.md#role-coordinator), [ROOT-09](architecture.md#role-coordinator) | С: внутреннее конструирование может делегироваться, владение графом остаётся определённым. |
| ROOT.R1 | 252-254 | Инициализация ESP-IDF event loop. | [ROOT-01](architecture.md#role-coordinator) | М: платформа инициализируется корнем через backend; ESP-specific loop не навязан host. |
| ROOT.R2 | 255 | Создание и lifetime Controller instances. | [ROOT-02](architecture.md#role-coordinator) | С. |
| ROOT.R3 | 256 | Создание Service с инъекцией State/controller references. | [ROOT-03](architecture.md#role-coordinator) | С. |
| ROOT.R4 | 257 | Создание Activity с State/service references. | [ROOT-04](architecture.md#role-coordinator) | С: зависимости — объявленные контракты, не чужие handlers. |
| ROOT.R5 | 258 | Регистрация всех event subscriptions. | [ROOT-05](architecture.md#role-coordinator), [DEL-01](architecture.md#role-delivery) | С/М: декларация может находиться в компоненте; завершённость wiring обеспечивает корень. |
| ROOT.R6 | 259 | Запуск задач в правильном порядке. | [ROOT-06](architecture.md#role-coordinator) | С/М: порядок владельцев/адаптеров сохраняется независимо от backend. |
| ROOT.R7 | 260 | Graceful shutdown. | [ROOT-07](architecture.md#role-coordinator) | С: обязательность сохранена; закрытие TCP-сессии не засчитывается как полный shutdown. |
| ROOT.N1 | 262-264 | Нет operational logic; только wiring/start. | [ROOT-08](architecture.md#role-coordinator) | С. |
| ROOT.N2 | 265 | Один Coordinator из app_main, не независимые вызовы из разных мест. | [ROOT-09](architecture.md#role-coordinator) | С/М: один корень на экземпляр; entry point выбирает backend. |
| DEL.D1 | 269-271 | Event-driven glue между слоями. | [Доставка, определение](architecture.md#role-delivery) | С. |
| DEL.D2 | 272 | Реализация на esp_event_loop. | [Доставка, определение](architecture.md#role-delivery), [ROOT-01](architecture.md#role-coordinator) | М: SDK callback/loop — платформенная граница; роль доставки portable. |
| DEL.R1 | 274-276 | Typed broadcast всем зарегистрированным subscribers. | [DEL-01](architecture.md#role-delivery), [DEL-04](architecture.md#role-delivery) | С: все адресаты учитываются; partial admission не выдаётся за полный успех. |
| DEL.R2 | 277 | Directed commands через typed command event либо direct target queue. | [DEL-02](architecture.md#role-delivery), [A05](architecture.md#a05) | С: разрешён объявленный адресный вход с полным admission, не произвольный обход. |
| DEL.R3 | 277 | Ссылка на event-model.md. | [Взаимодействие](architecture.md#interaction), [доставка](architecture.md#role-delivery) | К: нормативный смысл изложен самостоятельно; историческая ссылка baseline не редактируется. |
| DEL.R4 | 278 | Producer/consumer decoupling позволяет добавлять подсистемы без изменения их поведения. | [DEL-03](architecture.md#role-delivery), [зависимости](architecture.md#dependencies) | С/У: изменение wiring допускается и согласовано с ролью Coordinator; внутренности потребителя производителю не нужны. |
| DEL.N1 | 280-282 | Не передавать большие shared mutable structures, использовать State. | [DEL-05](architecture.md#role-delivery), [A08](architecture.md#a08) | С. |
| DEL.N2 | 283 | Callback не исполняет business logic; forward в owner queue и быстрый return. | [DEL-06](architecture.md#role-delivery), [A09](architecture.md#a09) | С. |

## Инварианты и схема взаимодействий

| ID | Строки baseline | Содержательное положение | Где сохранено | Итог |
|---|---|---|---|---|
| I00 | 287-289 | Правила обязательны и защищают coherent growth. | [Инварианты](architecture.md#invariants), [порядок решений](architecture.md#governance) | С: обязательны до явно принятой ревизии. |
| I01.1 | 291 | Shared State — SSOT. | [INV-01](architecture.md#invariants) | С. |
| I01.2 | 292 | Нельзя создавать private authoritative copies общего знания; оно живёт в Controller. | [INV-01](architecture.md#invariants), [A10](architecture.md#a10) | У: snapshot не запрещён; запрет конкурирующих mutable-источников согласован с 195/237. |
| I02.1 | 294 | Events — facts, commands — directed intent. | [INV-02](architecture.md#invariants), [взаимодействие](architecture.md#interaction) | С. |
| I02.2 | 295 | Obstacle/battery/Wi-Fi — примеры фактов; stop/start — намерений; не смешивать. | [INV-02](architecture.md#invariants), [admission/execution](architecture.md#admission-execution) | С/К: различие сохранено без требования конкретных подсистем. |
| I03.1 | 297 | Activity orchestrates, Service performs. | [INV-03](architecture.md#invariants) | С. |
| I03.2 | 298 | Activity не водит hardware; Service не выбирает system-wide modes. | [INV-03](architecture.md#invariants) | С. |
| I04.1 | 300 | Event callbacks thin. | [INV-04](architecture.md#invariants) | С. |
| I04.2 | 301 | SDK loop task чужой; нетривиальную работу переслать во вход владельца. | [INV-04](architecture.md#invariants), [A09](architecture.md#a09) | С/М: правило распространяется на любой backend callback. |
| I05.1 | 303 | Business logic в owner task, не event handler. | [INV-05](architecture.md#invariants) | С/М: owner context, не обязательная RTOS-task. |
| I05.2 | 304 | Обоснование: priority inversion, event-loop stack overflow, reentrancy. | [INV-05](architecture.md#invariants), [owner execution](architecture.md#owner-execution) | У: owner-context не доказывает отсутствие всех priority inversion/stack overflow; эти свойства требуют отдельной проверки. Сам запрет чужой обработки не ослаблен. |
| I06.1 | 306 | Не каждый класс/подсистема требует RTOS task. | [INV-06](architecture.md#invariants) | С. |
| I06.2 | 307 | Task имеет memory/scheduling cost; основания — independent execution, I/O, sampling, queue work. | [INV-06](architecture.md#invariants) | С: все основания и обе стоимости сохранены. |
| I06.3 | 307 | Controller не требует task. | [INV-06](architecture.md#invariants), [STATE-07](architecture.md#role-state) | С. |
| I07.1 | 309 | Clean foundation вместо premature framework complexity. | [INV-07](architecture.md#invariants) | С. |
| I07.2 | 310 | Не вводить generic DI, runtime service registries, тяжёлые templates ради гипотетической гибкости; explicit wiring. | [INV-07](architecture.md#invariants), [зависимости](architecture.md#dependencies) | С: речь о framework/service locator, не запрете предметного реестра фактов. |
| G01 | 314-320 | Activity: FSM/orchestration, свой task, snapshots/commands/events. | [Activity](architecture.md#role-activity), [схема](architecture.md#role-interactions) | С/М: owner context вместо обязательного FreeRTOS-примитива. |
| G02 | 321-323 | Activity → Service, directed commands. | [Схема](architecture.md#role-interactions), [ACT-03](architecture.md#role-activity) | С: через admission, не прямой handler. |
| G03 | 321-323 | Activity подписывается на events. | [Схема](architecture.md#role-interactions), [ACT-01](architecture.md#role-activity), [DEL-01](architecture.md#role-delivery) | С. |
| G04 | 324-328 | Service worker пишет State, публикует events, принимает commands. | [Схема](architecture.md#role-interactions), [SVC-01/02/03/04](architecture.md#role-service) | С. |
| G05 | 324-328 | Event Bus/esp_event_loop: broadcast facts и thin callbacks. | [Схема](architecture.md#role-interactions), [доставка](architecture.md#role-delivery) | С/М: SDK-реализация не навязана, обе обязанности сохранены. |
| G06 | 329-335 | Service читает/пишет State; State thread-safe, no task, SSOT. | [Схема](architecture.md#role-interactions), [State](architecture.md#role-state) | С. |
| G07 | 338 | Все объекты построены и связаны Coordinator/app_main. | [Схема](architecture.md#role-interactions), [Coordinator](architecture.md#role-coordinator) | С/М: entry point платформенный, корень единственный в экземпляре. |

## Уточнения, проверенные отдельно

1. **Контекст и задача.** Требования 196/215/319/326 сохранены как владение входом
   и последовательным исполнением. Конкретная FreeRTOS task относится к механизму:
   это согласовано с исходными A01/A06/A07 и инвариантом 6, а не разрешение
   исполнять предметную работу в чужом контексте.
2. **State и копии.** Запрет 292 не превращён в запрет snapshots, требуемых 195/237.
   Самостоятельный State-контракт, controlled writes, отсутствие поведения,
   событий и собственной задачи остаются обязательными.
3. **Broadcast и admission.** Назначением публикации остаются все зарегистрированные
   подписчики. Явный per-recipient admission описывает отказ при ограниченных
   ресурсах; не позволяет молча исключать получателей или считать Journal заменой
   событийного взаимодействия.
4. **Адресный вход.** Возможность direct target queue взята из исходной строки 277.
   Она не разрешает обход Dispatcher/маршрутизации: вход должен быть установленным
   маршрутом с теми же семантическими и admission-проверками. Публичная видимость
   произвольного метода не является архитектурным разрешением.
5. **Callbacks.** Адаптер выполняет ограниченную техническую работу на внешней
   границе. Предметная логика Activity/Service остаётся только у владельца.
6. **Причины owner-context.** Сохранено правило 303. Пояснение 304 не используется
   как безусловное доказательство отсутствия всех проблем приоритетов и стеков:
   такие гарантии не следуют из одного выбора контекста.
7. **Graceful shutdown.** Строка 260 перенесена как обязательное ROOT-07.
   Её выполнение не выводится из наличия close отдельной сессии.

## Общие коммуникационные дополнения

Они не заменяют ни одну исходную норму и не ограничивают архитектуру составом
конкретного приложения.

| Дополнение | Нормативное место | Связь с исходными принципами |
|---|---|---|
| Самостоятельность экземпляров, локальная маршрутизация, внешний device-oriented язык | COM-01 | A01, A03, A04, A05, Coordinator |
| Ingress без ожидания предметного исполнения; исходящий контракт подтверждений | COM-02, admission/execution | A04, A09, роли доставки и Service |
| Egress и сериализация записи при независимом размещении I/O | COM-03 | A06, A07, инвариант 6 |
| Владение lifecycle и защита от старых поколений при изменении State/ответе | COM-04/05 | A08, A10, Controller, Coordinator |
| Раздельные стадии времени, timeout не является NACK | COM-06 | A09, A11, A12, ответственность Service |
| Semantic retries, duplicates, readiness, keepalive у владельца контракта | COM-07 | A09, A11, Service |
| Remote State как наблюдение; identity/readiness/link/outcome различны | COM-08 | A03, A10, Controller |
| Явные capacity/overload/recovery и судьба принятой работы | COM-09 | A05, A08, A14 |
| Количество устройств — ресурсный профиль, не определение ролей | COM-10 | A01, A06, A14, инварианты 6/7 |
| Владение итогами конечной операции и отделение deadline исполнителя от наблюдателя | SVC-05/09, ACT-10, admission/execution | A09, A11, A12 |
| Critical как safety-critical контракт, Latest как разрешённая замена значения | Политики доставки | A03, A05, A08, A09 |

## Результат этапа

Все 14 аксиом, определения пяти исходных ролей, их обязанности и запреты,
семь инвариантов и связи исходной схемы имеют соответствие в нормативном документе.
Исторические утверждения о ветке, именах классов и предметном составе отделены
от содержащихся в них архитектурных свойств.

Не выявлено положения, для переноса которого необходимо отменить или ослабить
архитектурный принцип. Уточнения механизмов и пояснений перечислены явно выше.
Вопросы соответствия реализации этим требованиям не разрешаются этой таблицей:
независимый Architecture Compliance Review и любые исправления кода — отдельные
последующие этапы.
