# Raven ESP Runtime — Flight Controller Product Branch

This branch turns the Raven architectural substrate into a real ESP32-S3
product runtime connected to a Betaflight flight controller over a dedicated
UART using MSP.

The former navigation, joystick, manual-mode, and synthetic telemetry flows
were architecture probes. Their concrete implementations have been removed;
the reusable execution and adapter boundaries remain.

## Current vertical slice

```text
SpeedyBee UART
    -> EspUartPort
    -> MspClient
    -> BetaflightMspClient
    -> FlightControllerService
    -> FlightControllerState + link transition events
```

The first hardware milestone is intentionally read-only. The Betaflight client
exposes MSP identity and health requests only; no `MSP_SET_*` operation is
available.

## Preserved architecture

- `raven_core`: task, queue, payload ownership, and monitoring substrate.
- `raven_activities`: the Activity role and `BaseActivity`; concrete product
  activities will be added only when real orchestration exists.
- `raven_services`: the Service role and product subsystem workers.
- `raven_state`: task-safe canonical state stores.
- `raven_events`: broadcast facts and thin event callbacks.
- `raven_platform`: ESP-IDF hardware adapters plus reusable TCP primitives.
- `raven_gateways`: TCP and MSP protocol adapters, codecs, and registries.

## Hardware configuration

The FC link is disabled by default. Configure it with `idf.py menuconfig` under
`Raven Flight Controller` only after the physical UART pads, ESP32-S3 GPIOs,
logic levels, common ground, and power path have been verified.

## Host protocol tests

The MSP codec and request/response client are platform-independent and can be
tested without ESP-IDF:

```bash
g++ -std=c++17 -Wall -Wextra -Werror \
  -I components/raven_gateways/include \
  components/raven_gateways/src/msp_codec.cpp \
  tests/host/msp_codec_test.cpp \
  -o /tmp/raven_msp_codec_test && /tmp/raven_msp_codec_test

g++ -std=c++17 -Wall -Wextra -Werror \
  -I components/raven_gateways/include \
  -I components/raven_platform/include \
  components/raven_gateways/src/msp_codec.cpp \
  components/raven_gateways/src/msp_client.cpp \
  components/raven_gateways/src/betaflight_msp_client.cpp \
  tests/host/msp_client_test.cpp \
  -o /tmp/raven_msp_client_test && /tmp/raven_msp_client_test
```

The complete firmware build still requires the ESP-IDF toolchain.
