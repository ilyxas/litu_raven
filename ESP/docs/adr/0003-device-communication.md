# ADR-003: Devices, Communication Boundary, and Demo Cleanup

Date: 2026-11-26. Status: accepted for the current R&D; supersedes the layout and demo in ADR-002.

## Context

Validate Raven reuse across multiple physical ESPs without Arm Demo or temporary TCP code.
Message admission, protocol ACK/NACK, and execution remain separate contracts. MC BLE serves
as a service console with event-based output. The camera and a future iOS protocol are out of scope.

## Decision

Two ESP-IDF apps, MC and node, use shared components. The device-oriented ICD is independent of
Raven Core. RX is a platform task; TX is Component + OwnerTask. TcpManager owns socket lifecycle;
Session linearizes generation, admission, Peer State commits, and I/O leases. PeerService owns
handshake/keepalive, retries, and its API's deadlines. No general TransactionManager is introduced.

Inbox retains Ordered/Latest/Critical. Current contracts do not require Critical Ordered. Core growth
is limited to representation: payload 24→64 for two 64-byte typed envelopes, mask 8→16 for the MC
profile's nine endpoint IDs. Sizes are checked on target; the memory budget and hardware measurements
are recorded in validation.md.

Scaling is determined by resource configuration and platform capabilities. The number of boards
on the desk does not become an architectural limit. Current physical validation uses four devices;
MC-profile validation uses six concurrent TCP sessions.

Hardware measurements changed two initial lwIP settings: ESP-IDF 6.1.1 requires TCP_OVERSIZE with
SINGLE_PBUF, so short sends are bounded through queue/write thresholds. L2→L3 copy was enabled after
one unread socket reproducibly exhausted the shared Wi-Fi RX pool. Additional memory is explicitly
budgeted; isolation takes precedence over saving those buffers.

## Deliberately removed capabilities

The entire former `ESP/raven` tree is removed after moving the current core, OwnerTask, and BLE transport:

- ArmService, DualArmActivity, SimArmDriver, sensor/operation demo state, and domain-specific BLE commands;
- FC boot mode, Betaflight MSP client/codec, FlightControllerService/State/events, their configurations,
  and tests. **This decision ends ESP MSP/FC support**; it is not incidental removal of an unused file;
- old JSON/network-header/encoder-registry gateways, command/telemetry sockets, UART abstractions,
  unused BaseTask/EventBus/monitor layers, vendored cJSON, and the obsolete project map.

The Service/Activity model is not removed with the example. A new orchestration scenario can use
Component, Inbox, Executor, and the local Dispatcher. Restoring MSP, camera, UART, or iOS communication
requires a separate domain/transport adapter following the current boundaries.

iOS sources remain unchanged; ESP does not preserve legacy iOS wire compatibility. ADR-001/002
remain historical records with an explicit reference to the current architecture, not parallel
instructions for running the application.

## Verification

Host scenarios check admission/critical/tick, codec/framing, correlation and deadlines, stale
generations, concurrent close, bounded diagnostics, and the console parser. A separate node test
profile exercises the physical MC over Wi-Fi: capacity, duplicate identity, malformed/partial
frames, a non-reading peer, and recovery. Ordinary physical nodes are then checked with different
startup orders, power loss/return, and MC restarts. Current results and limitations are recorded
in [validation.md](../validation.md).
