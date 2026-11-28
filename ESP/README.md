# Raven: components, examples, and products

A reusable architecture, a device-communication reference example, and separate product directories:

- `components` — reusable core, ICD, communication, services, diagnostics, BLE, and platform adapters.
- `components/peripheral_drivers` — reserved for future minimal ESP-IDF peripheral wrappers; no drivers yet.
- `examples/device_communication/mc` — independent ESP-IDF project for ESP32-S3, Wi-Fi AP/TCP server and the `Raven-BLE` service console.
- `examples/device_communication/node` — independent ESP-IDF project for ESP32/ESP32-S3, Wi-Fi STA/TCP client. The camera is not initialized yet.
- `examples/device_communication/components/bootstrap` — reference composition, identity, endpoint assignment, lifetimes, and profile Kconfig. It is local to this example.
- `products/raven_chick/chick_mc` and `chick_body` — reserved independent product projects, each with its own `Bootstrap`, `Activities`, `Services`, and `Configuration`. Only directory placeholders exist; product firmware and build definitions are not implemented.
- `tests` — host/platform contract checks and hardware validation utilities.
- `tools`, `cmake`, and `docs` — build helper, communication profile settings, and specifications/technical documentation.

The reference projects discover both `ESP/components` and their example-local `components` directory.
Their common `sdkconfig.defaults` and `partitions.csv` live in `examples/device_communication`;
each project's defaults select its role. Product projects will select their own configuration and
composition while using reusable components; they do not depend on the reference bootstrap.

In the reference example, each board uses its factory MAC as its Device ID and a random Boot ID. There is no board list in code.
A new ordinary board uses `node` with a suitable `IDF_TARGET`, the same SSID/password, and the same port.
By default, MC reserves six concurrent sessions. This is a build resource profile, not a required
device count. Increasing the profile requires recalculating RAM, sockets, tasks, and AP limits.

## Build and flash the reference example

Verified SDK version: ESP-IDF 6.1.1. A standard exported IDF environment is required.

```sh
idf.py -C ESP/examples/device_communication/mc -B build/mc-esp32s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG="$PWD/build/mc-esp32s3/sdkconfig" build
idf.py -C ESP/examples/device_communication/node -B build/node-esp32 -DIDF_TARGET=esp32 -DSDKCONFIG="$PWD/build/node-esp32/sdkconfig" build
idf.py -C ESP/examples/device_communication/node -B build/node-esp32s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG="$PWD/build/node-esp32s3/sdkconfig" build
idf.py -C ESP/examples/device_communication/mc -B build/mc-esp32s3 -p PORT flash monitor
```

On this Windows machine, the helper below is available; environment variables can override SDK paths:

```powershell
./ESP/tools/idf.ps1 -App mc -Target esp32s3 -IdfArguments build
./ESP/tools/idf.ps1 -App node -Target esp32 -IdfArguments build
./ESP/tools/idf.ps1 -App node -Target esp32s3 -IdfArguments build
./ESP/tools/idf.ps1 -App node -Target esp32 -IdfArguments @('-p','COM7','flash')
```

Use a fresh build directory after moving from the old `ESP/apps` layout: CMake caches absolute
source paths. The Windows helper keeps its existing `-App mc|node` interface and places each
target's sdkconfig in its build directory. CI builds all three reference configurations and runs
the host/platform suite, including sanitizer jobs, for PRs targeting `product_raven_chick` or `dev_rnd`.

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
- [ADR-004: explicit roles and independent peer state](docs/adr/0004-explicit-roles-and-peer-state.md).

Arm Demo, FC/MSP, the old TCP/JSON/telemetry gateways, and ESP compatibility with the previous iOS protocol
have been removed. The iOS project is unchanged. Earlier experiments remain available in Git and ADR-001/002;
they do not describe the current firmware.
