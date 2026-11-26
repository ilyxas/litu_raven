# ADR-003: устройство, коммуникационная граница и очистка примера

Дата: 2026-11-26. Статус: принято для текущего R&D; заменяет размещение и демо из ADR-002.

## Контекст

Проверить переиспользуемость Raven на нескольких физических ESP без Arm Demo и временного TCP.
Приём сообщения, протокольный ACK/NACK и исполнение остаются разными контрактами. BLE MC нужен как
сервисный центр с событийным выводом. Камера и будущий iOS-протокол не входят в этап.

## Решение

Общие компоненты используются двумя ESP-IDF apps: MC и node. Device-oriented ICD независим от
Raven Core. RX — платформенная задача; TX — Component + OwnerTask. TcpManager владеет socket
lifecycle; Session linearizes generation, admission, Peer State commits и I/O leases. PeerService
владеет handshake/keepalive, retry и сроками своего API. Общий TransactionManager не введён.

Inbox сохраняет Ordered/Latest/Critical. Critical Ordered не нужен текущим контрактам. Ядро выросло
только в представлении: payload 24→64 для двух 64-byte typed envelopes, mask 8→16 для девяти endpoint
IDs MC-профиля. Размеры проверяются на target; memory budget и аппаратные замеры — в validation.md.

Масштабирование задаётся ресурсной конфигурацией и возможностями платформы. Число лежащих на столе
плат не становится архитектурным ограничением. Текущая физическая проверка — четыре устройства,
проверка MC-профиля — шесть одновременных TCP-сессий.

Измерение на железе изменило две первоначальные настройки lwIP: ESP-IDF 6.1.1 требует TCP_OVERSIZE
при SINGLE_PBUF, поэтому ограничение коротких отправок реализовано через queue/write thresholds.
L2→L3 copy включён после воспроизводимого исчерпания общего Wi-Fi RX pool одним непрочитанным
сокетом. Дополнительная память учтена явно; изоляция важнее экономии этих буферов.

## Сознательно удалённые возможности

Удаляется весь прежний `ESP/raven`, после переноса актуального ядра, OwnerTask и BLE transport:

- ArmService, DualArmActivity, SimArmDriver, sensor/operation demo state и специфичные BLE-команды;
- FC boot mode, Betaflight MSP client/codec, FlightControllerService/State/events, их конфигурации
  и тесты. **Поддержка MSP/FC в ESP этим решением прекращается**, это не удаление случайно неиспользуемого файла;
- старые JSON/network header/encoder registry gateways, command/telemetry sockets, UART абстракции,
  неиспользуемые BaseTask/EventBus/monitor слои, vendored cJSON и устаревшая карта проекта.

Модель Service/Activity не удаляется вместе с примером. Новый сценарий оркестрации сможет использовать
Component, Inbox, Executor и локальный Dispatcher. Возвращение MSP, камеры, UART или связи с iOS
потребует отдельного предметного/транспортного адаптера под актуальные границы.

iOS-исходники остаются без изменений; ESP не сохраняет прежний iOS wire compatibility. ADR-001/002
сохранены как история с явной ссылкой на текущую архитектуру, не как параллельная инструкция запуска.

## Проверка

Host-сценарии проверяют admission/critical/tick, codec/framing, корреляцию и сроки, stale generations,
конкурентный close, bounded diagnostics и console parser. Отдельный node test profile проверяет реальный
MC через Wi-Fi: capacity, duplicate identity, malformed/partial frames, non-reading peer и восстановление.
Обычные физические node затем проверяются с разным порядком запуска, потерей/возвратом питания и
перезапуском MC. Актуальные результаты и ограничения фиксируются в [validation.md](../validation.md).
