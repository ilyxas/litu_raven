# LITU / Raven: Continuation Context

Updated: November 26, 2026.

This is the entry point after an extended architectural discussion. It preserves the meaning of agreements, not a transcript or an independent implementation assignment. The current user request determines the work; new explicit instructions may change decisions recorded here.

Normative roles, contracts, and invariants are defined in the [Raven architectural specification](../ESP/docs/architecture.md). This handoff transfers context and does not replace the specification; distinguish historical information below from current requirements and implementation status.

## Collaboration and task framing

Ilya thinks aloud, explores examples, and uses them to explain architectural intent. Extract invariants and test them against other domains. An example becomes a concrete implementation requirement only when the task actually calls for it.

Ilya has practical experience with real-time system architecture, mission computers, asynchronous input, telemetry, simulation, and hardware. Discuss the work as colleagues. Challenge proposals on their merits: the user's experience does not make every suggestion unquestionable, and the agent should contribute its own technical judgment.

The user's preference is to keep the process light and make decisions confidently; the existing foundation may be reconsidered. This is engineering discretion within the task, not an instruction to rewrite the whole project or start external actions in every new chat. When implementation is requested, the user expects independent execution and verification.

A useful reasoning sequence is: example → required property → candidate solutions → chosen solution and rationale → verification. Do not turn an agent's interpretation into an established contract. Do not stall with questions whose answers can be found in code or chosen reasonably.

The conversational context includes different model voices and visual personas. A lively, warm, informal tone is acceptable; confident collaboration matters more than a formal interview style.

## Architectural intent

Raven is a system for behavior and subsystem interaction; ESP32/FreeRTOS is its current execution platform. Several operations can remain active concurrently. Roles and contracts must retain their meaning with a physical device, mock provider, or simulation.

| Discussion example | Required property |
|---|---|
| The left arm rises while the right wrist turns | Independent operations and goals may execute concurrently |
| An oven heats up or a turbine starts | The executor owns a long-running process and can react to its defined commands during that process |
| A device exposes only readable status | An operation can advance without callbacks through periodic servicing |
| Uploading photos over TCP | Execution details, retries, and deadlines are hidden behind the executor's API |
| Engine cut / HALT | Known critical messages receive priority; the recipient owns the response semantics |

The number of arms does not define the number of RTOS tasks. A turbine example does not require designing a turbine. The general architecture must not inherit domain-specific rules without a reason.

### Roles and responsibilities

- Activity orchestrates operations and scenarios. It trusts executor contracts, observes their states, and decides what to do next.
- Service owns its subsystem and operations: the accepted goal, progress, reactions to new messages, its own deadlines, and the outcome.
- State provides consistent state for other components. Internal details that others do not need are not required to become global data.
- Adapters translate external representations into known internal contracts. Application meaning must not depend on whether a message arrived through TCP, a mock, or another component.
- Service correctness and controllability are verified during development. Do not transfer its responsibility to the orchestrator by assuming an arbitrarily incorrect executor. Actual contract violations still need to be found and corrected.

### Delivery, ACK/NACK, and execution are separate levels

For the protocol under discussion, ACK/NACK confirms the outcome of message admission/internal delivery. Parsing, validation, and a dispatch attempt yield an immediate local result; the gateway forms a receipt without waiting for `handle_message()`, operation completion, or the next tick.

ACK itself does not mean movement has started or finished. Raven application ACK/NACK must also be distinguished from TCP transport ACK. The name of the acknowledgment does not require a blocking network API: the synchronous part here is the local delivery attempt's result.

Execution has a separate observable status. An initiator may poll it; a mandatory final-result push or additional acknowledgment of result receipt was not adopted as a universal requirement. A service must bring an accepted finite task to a defined terminal state according to its own contract. A long-running control session remains active until its defined termination condition.

The orchestrator does not guess an arm's or TCP upload's timeout and does not declare another owner's operation failed on its behalf. Losing a connection to an observer and execution failure are also different states.

### Ticks, callbacks, and new messages

`on_tick()` is a general periodic entry into owner behavior. It may read device status, track progress, check internal deadlines, and advance execution. It is not limited to a watchdog or to polling.

Devices may provide callbacks, readable status, or bounded synchronous calls. Each Service provides its own controllability contract. Physical-operation duration and individual handler-call duration are different concepts; do not impose one short execution deadline on all operations.

A callback passes data or a notification to the owner; behavioral changes remain in the designated owner context. An Activity may read service status on its tick without duplicating the services' internal deadlines.

### Critical messages and target replacement

Known message contracts/IDs determine criticality, which is respected at input and routing. The designated recipient handles the message. Only that recipient knows what to stop, cancel, and do next; navigation must not need to know another subsystem's cooling rules.

HALT is an example of such a message in this discussion. The architecture does not impose a mandatory Reset, a lockout until reboot, or a universal hardware reaction on every recipient. Buffering, priority, and delivery acknowledgment belong to the runtime/protocol; the domain reaction belongs to the owner.

A new target changes the current intent: movement need not finish the previous maneuver before applying it. Both API forms were agreed: finite operations with an outcome, including supersession, and continuing control with an updated target, such as a joystick. Specific target-compatibility rules belong to the service contract.

## Current stage: device communication

PR #3 was merged into `dev_rnd` (the current work's base is `02dd622`). The new stage was implemented
in the separate worktree `D:/Git/litu_raven_communication`, branch `codex/device-communication`;
the original shared checkout was not switched. [PR #4](https://github.com/ilyxas/litu_raven/pull/4)
targets `dev_rnd`. Final verification and resource measurements are in
[validation.md](../ESP/docs/validation.md); always check current PR/branch status in Git/GitHub.

Agreed: two ESP-IDF apps, MC/node, with shared Raven components; a device-oriented ICD; separate
TCP RX and TX, one writer per socket; a local Dispatcher; PeerService owns handshake/keepalive,
retries, and semantic deadlines. Session generation protects current Peer State as well as
responses. Timeout conditions and overload policy are described in the current documentation.

Board count does not define the architecture. Six sessions are MC's current resource profile;
physical validation was agreed for four ESPs. A new board receives a factory-MAC Device ID and a
random Boot ID; no new classes/address lists are needed. The camera is not initialized. BLE remains
on MC as an event-based service console, without streaming telemetry or Arm commands.

Cleanup deliberately removes Arm Demo, old TCP/JSON gateways, the FC/MSP capability and its boot
mode, obsolete layers, and vendored cJSON. iOS code is unchanged; previous ESP compatibility with it
is not preserved. Future iOS integration, MSP, UART, and camera services are separate tasks under
the current boundaries.

Inbox Ordered/Latest/Critical and Executor scheduling are preserved. Critical is safety-critical
coalescing according to contract; ACK and diagnostics do not use it. Payload size and local
addressing were increased for the actual typed envelopes and MC composition, with resource checks.
A new architectural entity is allowed when it simplifies a clear contract; core revision is not forbidden.

November 26 stage verification: 17 host scenarios, ASan/UBSan/TSan, and three firmware targets in CI;
a real six-session TCP suite with BLE connected; four physical boards with independent restarts and
a stable 30-minute soak. Ordinary firmware without test monitor/probe was left on COM4/5/6/7, and
the shared handshake was confirmed after flashing. Ports were released; always recheck current COM assignments.

## Completed-stage history (before November 26)

Earlier results and class placement are retained below as history; old demo commands/paths are
not instructions for the current firmware.

This is an R&D direction separate from `main`. The user requested committing the architecture/embedded skills first, then creating `dev_rnd` and a working branch from it, implementing and testing the prototype, and opening a PR into `dev_rnd`.

Verified Git and review milestones as of November 22, 2026:

- R&D base: `dev_rnd`; the branch was created after skills commit `18fa13f`.
- Prototype implementation branch: `codex/raven-execution-prototype`.
- Prototype implementation: `e4a0702` — `Implement owner execution runtime and dual-arm RND prototype`.
- Independent-review fixes: `2d80a9f` — `Fix child result retention, command reconnects, and retarget timing`.
- [PR #2](https://github.com/ilyxas/litu_raven/pull/2) **was merged into `dev_rnd` on November 22, 2026**, merge commit `e998135`. The merge was confirmed by PR state and `origin/dev_rnd`; it is a separate fact from completion of review.
- Agent instructions and the initial handoff were added in `1a6b686`. That commit and later documentation changes are not part of merge `e998135`; their inclusion in `dev_rnd` must be checked separately.
- These facts are reference points, not permanent current truth. Before continuing, check branches, changes, PRs, and actual verification results.

After PR #2, the user requested distributing classes among architectural components and enabling BLE for the existing Bluetooth Serial Connect app. The implementation was committed in `f7e54ae`; [PR #3](https://github.com/ilyxas/litu_raven/pull/3) was opened into `dev_rnd`. Work used the separate worktree `D:/Git/litu_raven_ble`, branch `codex/runtime-components-ble` from `origin/dev_rnd`; the shared `D:/Git/litu_raven` checkout was not switched. Handoff commits `1a6b686`/`f752f4b` were carried into this branch as `67ea59d`/`c0d5414`.

Components `raven_runtime`, `raven_rnd`, and `raven_rnd_esp` were removed: the core moved to `raven_core`, state to `raven_state`, arms and the sensor example to `raven_services`, the scenario to `raven_activities`, OwnerTask/drivers/radio/sockets to `raven_platform`, and TCP/BLE adapters to `raven_gateways`. Shared messages and the driver interface are in `raven_contracts`; composition is in `main/demo_runtime.hpp` and `demo_bootstrap.cpp`; tests are in `tests`. Old `RAVEN_RND_*` Kconfig names were retained for compatibility. Two virtual arms and concrete capacities/periods remain verification examples, not universal requirements.

The app's BLE profile was confirmed by its author's documentation: FFE0/FFE1, LF-terminated lines, console `C:...`, and D/L/I/G responses/widgets. ESP advertises `Raven-BLE`. Delivery/ACK and execution remain separate; BLE reconnect does not cancel already delivered operations. The new `partitions.csv` keeps NVS/PHY at their old addresses and gives the application `0x1f0000` bytes: Wi-Fi+BLE exceeds the former 1 MiB. Default builds enable NimBLE and native USB console; boot tests run only when `RAVEN_RND_SELF_TEST` is enabled. When tests are enabled, BLE starts after them.

ESP is connected; the user authorized build, flash, and verification as part of the implementation task. Check the actual port, configuration, and board state before working. Reading this document alone does not authorize flashing a device or continuing an old task.

iOS is a source of future requests/joystick input and a consumer of state for the scene. The user explicitly stated that existing iOS code must not be changed now; it is a separate task. Account for the current integration boundary. MSP is less important and serves as an example built on the architecture.

A side chat performed the [first independent review](https://github.com/ilyxas/litu_raven/pull/2#pullrequestreview-6009722052) and [follow-up review of fixes](https://github.com/ilyxas/litu_raven/pull/2#pullrequestreview-6009856677). The findings—loss of a child result when history was evicted, an old client blocking new TCP sessions, and lost movement time on retarget—were fixed in `2d80a9f`; all three threads were resolved. No further findings requiring correction were identified in the reviewed change. Technical rationale and repeat-verification results are retained in the PR. Future reviews should continue checking general contracts and the boundary between the core and the domain demo.

## Where to find technical references

- [Current applications and build commands](../ESP/README.md).
- [Raven architectural specification](../ESP/docs/architecture.md).
- [Immutable baseline from 02dd622](../ESP/docs/architecture-baseline-02dd622.md).
- [Specification completeness review](../ESP/docs/architecture-coverage.md).
- [Communication implementation, Session, and generations](../ESP/docs/communication-architecture.md).
- [ICD and timing semantics](../ESP/docs/communication.md).
- [BLE console](../ESP/docs/ble-console.md).
- [Resources and verification](../ESP/docs/validation.md).
- [ADR-003: communication and cleanup](../ESP/docs/adr/0003-device-communication.md).
- [Historical ADR-002: components and BLE](../ESP/docs/adr/0002-role-components-and-ble-console.md).
- [ADR-001: delivery and execution](../ESP/docs/adr/0001-delivery-and-execution.md).
- [iOS direction](../IOS/docs/architecture_note_draft.md).
- [PR #1](https://github.com/ilyxas/litu_raven/pull/1): removed NavigationActivity, PilotInputService, and telemetry illustrate earlier intent. Their code is available in pre-merge history; it is material for understanding, not a mandatory template.

This summary does not replace reading the code being changed. Confirmed verification of `2d80a9f` occurred during review-fix work: ESP-IDF 6.1.1 build, ESP32-S3 flash, and **20/20 PASS on the board**—18 deterministic scenarios and two TCP tests over lwIP loopback. Before the fixes, four new regressions reproduced the defects while the previous 15 tests passed. After the fixes, TCP-session replacement took 12–13 ms, and another 15 seconds of observation showed no restarts or watchdog. These are results from an earlier hardware run, not a claim of a new run whenever the handoff is read.

The follow-up independent review examined code, tests, and saved logs; it independently repeated `git diff --check` and Python syntax checks but did not rerun C++/MCU tests. Native host C++ tests, real Wi-Fi/iPhone sessions, and keepalive expiration after radio loss had not yet been checked at that stage. Keep these evidence levels separate in subsequent reports.

## Additional context on agent autonomy

In the shared [VideoRouter API Studio](https://github.com/ilyxas/videorouterApiUI) project, the model independently implemented Grok CLI as the provider for Image/Video cards, and Ilya uses the resulting tool. This illustrates the expected initiative: understand the goal, choose a mechanism, implement, verify, and deliver something usable.

In the examined code, the adapter defines the scope of one card for the agent, receives an event stream, and imports the result; execution details are hidden from the rest of the workflow. The [workflow construction contract](https://github.com/ilyxas/videorouterApiUI/blob/main/contracts/workflow/README.md) accepts ordinary user language and assigns graph construction to the agent. These references provide collaboration context; there is no need to port that project or run its generators for Raven work.

## Maintaining this entry point

`AGENTS.md` contains concise working rules and points here. This document preserves stable agreements, the documentation map, and explicitly dated status. Technical details are maintained in the corresponding subsystem documents without parallel contradictory descriptions.

After a significant new decision, update the relevant text while distinguishing the user's decision from the implementation chosen by the agent. Do not add a transcript, personal contacts, secrets, or a separate report for every run. This handoff may be explicitly supplied as context to a new chat in another project; its branches and constraints apply specifically to `litu_raven`.

## Component reorganization and BLE verification (November 22, 2026)

ESP32-S3 v0.2, 16 MB flash, was available on COM4 through built-in USB-Serial/JTAG. With user authorization, the ESP-IDF Monitor processes holding COM4 were closed. ESP-IDF 6.1.1 firmware was built and flashed from the new worktree; **25/25** passed on the board: 23 deterministic scenarios and two lwIP loopback tests. All 20 earlier checks were retained. TCP takeover in the final run took 13–14 ms; a slow consumer caused a real send failure without stopping services. After the tests, BLE_READY and 15 seconds without fault/reset/watchdog were observed; free heap was about 135 KB, minimum 102 KB while the test task was also running. Verification build: 1 077 424 bytes, 47% of the app partition free. Minimum free stack: services >3 KB, Activity 2956, TCP 3284, telemetry 2304; after BLE startup, gateway >4 KB and NimBLE host 2336 bytes.

The alternate FC boot mode without BLE was also built but not flashed. Host C++ tests had not yet run on the computer; these scenarios ran on ESP. The user confirmed connection through another BLE app, then through the original Bluetooth Serial Connect app. ESP recorded a GAP connection, FFE1 subscription, and outbound notifications; minimum free host stack after connection was 1656 bytes. The user reported noisy logs: widget updates were limited to changes, `watch off/on` was added, and INFO logging for every NimBLE notification was disabled. The final 25/25 run includes quiet-readout checks: repeated identical state is silent, `watch off` preserves command responses, and `watch on` restores fresh readings. Run logs are in the new worktree's ignored build directory.

The board was left with ordinary final firmware and boot tests disabled: 1 062 656 bytes, 48% of the app partition free. Verified flash writing was followed by 20 seconds of observation: BLE_READY at 417 ms, no test commands, and no fault/reset/watchdog. Free heap was 136 704 bytes, minimum 132 364, largest block 90 112; BLE gateway free stack was 4788 and host free stack 2336 bytes before a new connection. The final FC variant was built again without BLE. The serial port was released. Use `watch off` for a quiet console and `watch on` for the panel; reconnect restores on.
