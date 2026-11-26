# ADR-002: Role components and a separate BLE text boundary

Status: Implemented in the R&D work branch; targets `dev_rnd`.

Historical record, superseded for component layout and demo behavior by
[ADR-003](0003-device-communication.md). FFE0/FFE1 transport remains; use the
[current service console](../ble-console.md) rather than the former arm commands.

## Context

The initial behavior was grouped in prototype-only components and a large ESP
composition file. It obscured the existing Activity/Service/State/Platform roles.
The user requested a structural cleanup and direct use of Bluetooth Serial Connect
before further gateway integration. Its documented profile uses FFE0/FFE1 and
newline ASCII; Raven's current TCP client uses a binary envelope.

## Decision

- Place portable delivery/execution classes in `raven_core`, service and Activity
  behavior in their respective components, state in `raven_state`, and device/
  FreeRTOS/socket/BLE backends in `raven_platform`. Keep one meaningful class per
  file, with small related value types beside it.
- Introduce a header-only `raven_contracts` component for demo message contracts
  and `IArmDriver`. Gateways depend on contracts and snapshots, without depending
  on service implementations (the legacy FC service already depends on gateways).
- Keep `main/demo_runtime.hpp` as the lifetime/composition root and separate
  bootstrap from runtime behavior. Move portable and board tests into `tests`.
- Implement NimBLE FFE0/FFE1 as a bounded byte transport. A gateway task owns the
  line parser and demo channel bindings. GATT callbacks copy input; only service
  owners mutate actuator behavior. NimBLE host context owns connection state and
  notification fragmentation. Session IDs prevent stale output crossing reconnects.
- Preserve synchronous inbox admission ACK/NACK and service-owned execution.
  BLE replies use D0 text; periodic readouts are snapshots, not completion ACKs.
  A disconnected or overloaded transport does not cancel accepted service work.
- Preserve TCP wire formats and old FC boot mode. Do not modify iOS code or
  simultaneously migrate the older NetworkGateway/codec interfaces.

## Alternatives and consequences

Keeping an additional RND component would preserve the organizational problem.
Putting demo commands in core would couple the general executor to arm semantics;
shared contracts avoid that dependency at the cost of one small component.

NimBLE supplies BLE without an external module. Nordic UART would require app
UUID configuration; the author's default HM-10 profile works with its defaults.
Wi-Fi and BLE increase flash/RAM use: the application partition grows from 1 MiB
to `0x1f0000`, preserving existing NVS/PHY addresses. Bounded queues avoid unbounded
memory use; saturation disconnects the session instead of silently skipping bytes.
The default is a local, unpaired virtual-device workbench, not a hardware control
security policy. GPIO actuation and production access control remain outside it.

## Verification

Retain previous runtime/loopback regressions and add ASCII framing, validation,
channel HALT under flood and reconnect tests. Compile both demo and alternate FC
boot modes. Verify firmware boot and BLE advertising on the ESP; distinguish those
results from real phone/GATT testing in the task's final report and handoff.
