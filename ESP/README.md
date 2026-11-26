# Raven: device communication

A shared runtime and two independent ESP-IDF applications:

- `apps/mc` — ESP32-S3, Wi-Fi AP/TCP server and the `Raven-BLE` service console.
- `apps/node` — ESP32/ESP32-S3, Wi-Fi STA/TCP client. The camera is not initialized yet.
- `components` — shared core, ICD, communication, services, and platform adapters.
- `tests` — host contract checks and a test client for a physical MC.

Each board uses its factory MAC as its Device ID and a random Boot ID. There is no board list in code.
A new ordinary board uses `node` with a suitable `IDF_TARGET`, the same SSID/password, and the same port.
By default, MC reserves six concurrent sessions. This is a build resource profile, not a required
device count. Increasing the profile requires recalculating RAM, sockets, tasks, and AP limits.

## Build and flash

Verified SDK version: ESP-IDF 6.1.1. A standard exported IDF environment is required.

```sh
idf.py -C ESP/apps/mc -B build/mc -DIDF_TARGET=esp32s3 build
idf.py -C ESP/apps/node -B build/node -DIDF_TARGET=esp32 build
idf.py -C ESP/apps/mc -B build/mc -p PORT flash monitor
```

On this Windows machine, the helper below is available; environment variables can override SDK paths:

```powershell
./ESP/tools/idf.ps1 -App mc -Target esp32s3 -IdfArguments build
./ESP/tools/idf.ps1 -App node -Target esp32 -IdfArguments build
./ESP/tools/idf.ps1 -App node -Target esp32 -IdfArguments @('-p','COM7','flash')
```

`menuconfig → Raven device profile` contains role, capacity, network, and test-mode settings.
The application partition fits in 2 MiB flash; PSRAM is not required. MC uses the native USB console;
an ordinary ESP32 node uses UART. For an ESP32-S3 node, select USB Serial/JTAG console if needed.
Test modes are disabled by default.

Development network: `Raven-Link`, password `raven-link`, TCP `8080`. This is an open, trusted test bench:
Device ID detects conflicts but does not authenticate a device. Product network security is a separate contract.

## Verification and documentation

```sh
cmake -S ESP/tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

- [Raven architectural specification](docs/architecture.md).
- [Immutable architecture baseline from 02dd622](docs/architecture-baseline-02dd622.md).
- [Specification completeness against the baseline](docs/architecture-coverage.md).
- [Communication implementation and lifecycle](docs/communication-architecture.md).
- [ICD, framing, and timing semantics](docs/communication.md).
- [BLE console](docs/ble-console.md).
- [Resources and reproducible verification](docs/validation.md).
- [ADR-003: boundaries and deliberate cleanup](docs/adr/0003-device-communication.md).

Arm Demo, FC/MSP, the old TCP/JSON/telemetry gateways, and ESP compatibility with the previous iOS protocol
have been removed. The iOS project is unchanged. Earlier experiments remain available in Git and ADR-001/002;
they do not describe the current firmware.
