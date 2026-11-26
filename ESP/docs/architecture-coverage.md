# Architectural Specification Completeness Review

## Scope

The immutable [baseline](architecture-baseline-02dd622.md) is compared with the
[normative Raven specification](architecture.md). This review addresses the document's
semantic completeness: axioms, definitions, responsibilities, prohibitions, invariants,
and interactions. It is not an Architecture Compliance Review of the PR implementation.
It does not claim that current code satisfies new or preserved requirements.

The concrete communication implementation is described in
[communication-architecture.md](communication-architecture.md).
That description does not replace the specification and was not used as a reason
to weaken an architectural rule. The handoff is not a source of normative requirements.

## Immutable source

- Commit: `02dd622f5e2e08fa8f0463dcef9ae21358191f7b`.
- Original path: `ESP/raven/docs/architecture.md`.
- Copy: `ESP/docs/architecture-baseline-02dd622.md`.
- Git blob of both source and copy: `f9aa0dd686e92f764253727abd8ea9fdf049f1cd`.
- SHA-256: `c2afd8f1ebd9e594b4caa3c2f09b7cc4a5b4e43f70334efe5365bc171bd0faa4`.
- Size: **16 356 bytes**, **341 lines**, original LF endings.
- The copy was committed separately in `fbdb7ac`; line-ending conversion is disabled
  for it through the local `.gitattributes`.

The baseline must not be edited, shortened, reformatted, translated, or updated.
Even historical links remain byte-identical to the source; they are not instructions
for running the current application. Provenance metadata and the comparison belong
in this file, not in the baseline.

Reproducible byte-for-byte verification from the repository root:

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

## Reading the comparison

Line numbers always refer to the baseline. Lines containing multiple independent
claims have separate entries. Listed sources, functions, and prohibitions are also
checked individually; repeated requirements in the invariants or diagram are not skipped.

- **P** — architectural meaning preserved.
- **M** — meaning preserved; the description of a concrete mechanism/backend changes.
- **C** — historical context or domain example; its architectural property is
  explicitly present at the cited location in the specification.
- **U** — ambiguity or an inaccurate explanation clarified without weakening the rule.

Headings, blank lines, diagram fences, and horizontal separators add no requirements;
the ranges reflect their thematic scope. ACT/SVC/STATE/ROOT/DEL identifiers refer to
specific clauses in the corresponding section. A01–A14 and INV-01–INV-07 retain the
original numbering.

## Introduction and interpretation

| ID | Baseline lines | Substantive claim | Preserved in | Outcome |
|---|---|---|---|---|
| P01 | 1 | The document describes Raven's architecture as a whole. | [Purpose](architecture.md#scope) | P: general normative scope. |
| P02 | 3-5 | Delivery/execution contracts have a separate description connected to the general model. | [Admission and execution](architecture.md#admission-execution), [communication](architecture.md#communication-extension) | C: prototype links remain in the baseline; current general contracts are stated independently. |
| P03 | 5 | The role model remains in force. | [Purpose](architecture.md#scope), [A02](architecture.md#a02) | P: roles do not depend on the set of instantiated components. |
| P04 | 6 | The FC composition is available as an alternate boot mode. | [A14](architecture.md#a14), [adapters](architecture.md#role-adapter) | C: a fact about a particular composition is not turned into a general obligation to support FC. |
| P05 | 8 | The conceptual model, role boundaries, and invariants are mandatory for contributors. | [Purpose](architecture.md#scope), [invariants](architecture.md#invariants) | P. |
| P06 | 8 | Architecture guides code interpretation despite temporary shortcuts and experiments. | [Purpose](architecture.md#scope), [decision process](architecture.md#governance) | P: a code deviation does not redefine the rule. |
| P07 | 14 | Navigation, synthetic telemetry, joystick, and manual mode are architectural probes. | [A14](architecture.md#a14) | C: domain names do not restrict the general meaning of probes. |
| P08 | 14 | Probes test responsiveness, stress, ownership, routing, and message/tick interaction. | [A14](architecture.md#a14), [execution](architecture.md#owner-execution) | P: all five properties are present. |
| P09 | 14 | Removing examples preserves the roles and contracts they exercised. | [A14](architecture.md#a14) | P. |
| P10 | 14 | The first physical slice is Betaflight UART/MSP. | [A04](architecture.md#a04), [A13](architecture.md#a13), [role interactions](architecture.md#role-interactions) | C: a particular slice, not a definition of Raven. |
| P11 | 16-18 | The branch's direction differs from extending the former demo. | [Purpose](architecture.md#scope), [A14](architecture.md#a14) | C: a stage or branch does not define the architectural model. |
| P12 | 18 | Reusable mechanisms remain meaningful without a current consumer. | [A06](architecture.md#a06), [A07](architecture.md#a07), [A14](architecture.md#a14) | P: the need for a concrete class is separate from the existence of a capability. |
| P13 | 18 | The absence of a concrete Activity does not invalidate Activity. | [A02](architecture.md#a02), [Activity](architecture.md#role-activity), [A14](architecture.md#a14) | P: the BaseActivity name is not a prerequisite for the role. |
| P14 | 18 | TCP gateways remain valid adapters during a UART/MSP slice. | [A03](architecture.md#a03), [A04](architecture.md#a04), [adapters](architecture.md#role-adapter) | P/M: transport substitutability is preserved without requiring a particular gateway class. |
| P15 | 20-28 | UART adapter → framing/gateway → semantic client → Service owner → State/events. | [A04](architecture.md#a04), [Service](architecture.md#role-service), [role interactions](architecture.md#role-interactions) | C/M: all roles in the flow are preserved; the domain protocol is not imposed on the architecture. |
| P16 | 30 | Any layer may be revised; hardware tests reveal execution, ownership, and adapter issues. | [Decision process](architecture.md#governance), [A14](architecture.md#a14) | P: revision is explicit; a test does not automatically change a rule. |

## All 14 axioms

| ID | Baseline lines | Substantive claim | Preserved in | Outcome |
|---|---|---|---|---|
| A01.1 | 32-36 | Raven is a platform-independent behavioral system; ESP/FreeRTOS is a backend. | [A01](architecture.md#a01) | P. |
| A01.2 | 37 | Roles and contracts apply in simulation. | [A01](architecture.md#a01), [A13](architecture.md#a13) | P. |
| A01.3 | 38 | Desktop execution is legitimate. | [A01](architecture.md#a01) | P. |
| A01.4 | 39 | Hardware is optional at the architectural level. | [A01](architecture.md#a01) | P. |
| A01.5 | 40 | Platform-specific details belong below the behavioral core. | [A01](architecture.md#a01), [dependencies](architecture.md#dependencies) | P. |
| A02.1 | 42-44 | Activity, Service, and State express behavior and are not ESP constructs. | [A02](architecture.md#a02) | P. |
| A02.2 | 45 | Activity owns orchestration and mode logic. | [A02](architecture.md#a02), [Activity](architecture.md#role-activity) | P. |
| A02.3 | 46 | Service owns subsystem work. | [A02](architecture.md#a02), [Service](architecture.md#role-service) | P. |
| A02.4 | 47 | State owns canonical truth. | [A02](architecture.md#a02), [State](architecture.md#role-state) | P. |
| A02.5 | 49 | A platform dependency inside behavior is leakage, not intended design. | [A02](architecture.md#a02) | P: a deviation requires an explicit decision; existing code does not legitimize it. |
| A03.1 | 51-53 | Message origin does not define its domain meaning. | [A03](architecture.md#a03) | P. |
| A03.2 | 54 | TCP is a valid source. | [A03](architecture.md#a03) | P. |
| A03.3 | 55 | RF control is a valid source. | [A03](architecture.md#a03) | P. |
| A03.4 | 56 | Mock input is a valid source. | [A03](architecture.md#a03) | P. |
| A03.5 | 57 | Simulation is a valid source. | [A03](architecture.md#a03) | P. |
| A03.6 | 58 | Synthetic sensors are a valid source. | [A03](architecture.md#a03) | P. |
| A03.7 | 59 | Local generation is a valid source. | [A03](architecture.md#a03) | P. |
| A03.8 | 61-62 | Semantic meaning matters. | [A03](architecture.md#a03) | P. |
| A03.9 | 63 | The payload contract matters. | [A03](architecture.md#a03), [A05](architecture.md#a05) | P. |
| A03.10 | 64 | Route legitimacy matters. | [A03](architecture.md#a03), [A05](architecture.md#a05) | P. |
| A03.11 | 65 | Handling ownership matters. | [A03](architecture.md#a03), [A09](architecture.md#a09) | P. |
| A03.12 | 67 | A component reacts to a contract, not a transport path. | [A03](architecture.md#a03) | P: semantic identity is not replaced with transport knowledge. |
| A04.1 | 69-71 | Wire formats, framing, msg_id, codecs, and gateways are boundary mechanisms. | [A04](architecture.md#a04) | P/M: functions remain without a mandatory class name. |
| A04.2 | 72 | Accept an external representation. | [A04](architecture.md#a04), [ADP-01](architecture.md#role-adapter) | P. |
| A04.3 | 73 | Validate the representation. | [A04](architecture.md#a04), [ADP-01](architecture.md#role-adapter) | P. |
| A04.4 | 74 | Translate into an internal semantic contract. | [A04](architecture.md#a04), [ADP-01](architecture.md#role-adapter) | P. |
| A04.5 | 75 | Deliver to the appropriate owner. | [A04](architecture.md#a04), [A05](architecture.md#a05) | P. |
| A04.6 | 77 | Transport origin does not become behavioral meaning. | [A03](architecture.md#a03), [A04](architecture.md#a04) | P. |
| A05.1 | 79-83 | Routing is strict admission, not generic packet forwarding. | [A05](architecture.md#a05) | P. |
| A05.2 | 84 | The message is known. | [A05](architecture.md#a05) | P. |
| A05.3 | 85 | The payload matches its contract. | [A05](architecture.md#a05), [DEL-07](architecture.md#role-delivery) | P: size alone is not treated as full semantic validation. |
| A05.4 | 86 | A decoder/adapter exists. | [A05](architecture.md#a05) | M/U: external input is decoded; a formed internal message does not require repeated wire decoding. |
| A05.5 | 87 | The route is explicitly registered. | [A05](architecture.md#a05), [ROOT-05](architecture.md#role-coordinator) | P. |
| A05.6 | 88 | The recipient is intended to receive the message. | [A05](architecture.md#a05), [DEL-07](architecture.md#role-delivery) | P. |
| A05.7 | 90 | An unprepared component does not receive the data. | [A05](architecture.md#a05) | P. |
| A06.1 | 92-94 | Roles are defined by responsibility, not RTOS primitives. | [A06](architecture.md#a06) | P. |
| A06.2 | 96 | Activity is an orchestration role. | [A06](architecture.md#a06) | P. |
| A06.3 | 97 | Service is a subsystem execution role. | [A06](architecture.md#a06) | P. |
| A06.4 | 99 | Task/queue/tick/scheduling sit below roles; the domain does not think in FreeRTOS terms. | [A06](architecture.md#a06), [backend](architecture.md#role-platform) | P. |
| A07.1 | 101-103 | The execution mechanism hosts roles rather than defining them. | [A07](architecture.md#a07) | P/M: BaseTask is not an architectural obligation. |
| A07.2 | 104 | Asynchronous execution. | [A07](architecture.md#a07), [owner execution](architecture.md#owner-execution) | P. |
| A07.3 | 105 | Queue-driven delivery. | [A07](architecture.md#a07), [DEL-02](architecture.md#role-delivery) | P. |
| A07.4 | 106 | Periodic ticks. | [A07](architecture.md#a07), [A12](architecture.md#a12) | P. |
| A07.5 | 107 | Payload lifetime handling. | [A07](architecture.md#a07), [A08](architecture.md#a08) | P. |
| A07.6 | 108 | Separation of component contexts. | [A07](architecture.md#a07), [A09](architecture.md#a09) | P: address-space isolation is not claimed. |
| A07.7 | 110 | Activity/Service are expressed above task machinery. | [A06](architecture.md#a06), [A07](architecture.md#a07) | M: BaseActivity/BaseService responsibilities are separated from historical inheritance. |
| A08.1 | 112-114 | Self-containment at enqueue; a queue does not automatically perform an arbitrary deep copy. | [A08](architecture.md#a08) | P/M: required ownership is explicit; the copying mechanism is not prescribed. |
| A08.2 | 116-117 | Independence from sender-buffer lifetime. | [A08](architecture.md#a08) | P. |
| A08.3 | 118 | The payload stays valid until handling completes. | [A08](architecture.md#a08) | P. |
| A08.4 | 119 | Inter-task delivery is memory-safe by contract. | [A08](architecture.md#a08) | P: copying a pointer is insufficient. |
| A09.1 | 121-123 | Meaningful work executes in owner context. | [A09](architecture.md#a09) | P. |
| A09.2 | 124 | Not in an event callback. | [A09](architecture.md#a09), [DEL-06](architecture.md#role-delivery) | P. |
| A09.3 | 125 | Not in transport ingress. | [A09](architecture.md#a09), [COM-02](architecture.md#communication-extension) | P. |
| A09.4 | 126 | Not in a foreign task. | [A09](architecture.md#a09) | P. |
| A09.5 | 128 | Multiple sources do not change ownership of decisions or subsystem work. | [A09](architecture.md#a09), [owner execution](architecture.md#owner-execution) | P. |
| A10.1 | 130-132 | Data needed by multiple components belongs in State, not scattered local details. | [A10](architecture.md#a10), [SVC-08](architecture.md#role-service) | P. |
| A10.2 | 134 | Activity/Service read State and update it through controlled ownership paths. | [A10](architecture.md#a10), [ACT-07](architecture.md#role-activity), [STATE-01/02](architecture.md#role-state) | P. |
| A10.3 | 134 | State is canonical shared system knowledge. | [A10](architecture.md#a10), [State](architecture.md#role-state) | P. |
| A11.1 | 136-140 | Activity determines what the system should do. | [A11](architecture.md#a11), [ACT-02](architecture.md#role-activity) | P. |
| A11.2 | 141 | Service performs subsystem work. | [A11](architecture.md#a11) | P. |
| A11.3 | 142 | State stores system truth. | [A11](architecture.md#a11) | P. |
| A11.4 | 143 | Adapters connect the core to the outside world. | [A11](architecture.md#a11), [adapters](architecture.md#role-adapter) | P. |
| A11.5 | 145 | Activity does not perform low-level subsystem work. | [A11](architecture.md#a11), [ACT-06/08](architecture.md#role-activity) | P. |
| A11.6 | 145 | Service does not make system-wide orchestration decisions. | [A11](architecture.md#a11), [SVC-06](architecture.md#role-service) | P. |
| A12.1 | 147-149 | Tick is a first-class execution mechanism, not a workaround. | [A12](architecture.md#a12) | P. |
| A12.2 | 151 | An Activity may change policy on its tick while the runtime operates coherently. | [A12](architecture.md#a12) | P. |
| A12.3 | 152 | Message-driven behavior. | [A12](architecture.md#a12) | P. |
| A12.4 | 153 | Time-driven behavior. | [A12](architecture.md#a12) | P. |
| A12.5 | 155 | Policy loops, supervision, autonomy, and sensor-driven behavior. | [A12](architecture.md#a12) | P: all four purposes are retained. |
| A13.1 | 157-159 | Hardware is not the only provider. | [A13](architecture.md#a13) | P. |
| A13.2 | 160 | Physical sensors. | [A13](architecture.md#a13) | P. |
| A13.3 | 161 | Simulated sensors. | [A13](architecture.md#a13) | P. |
| A13.4 | 162 | Mock devices. | [A13](architecture.md#a13) | P. |
| A13.5 | 163 | Synthetic control streams. | [A13](architecture.md#a13) | P. |
| A13.6 | 164 | Replayed data. | [A13](architecture.md#a13) | P. |
| A13.7 | 166 | The behavioral core remains valid across providers; only the lower layer knows whether input is real. | [A13](architecture.md#a13), [PLAT-03](architecture.md#role-platform) | P. |
| A14.1 | 168-170 | Concrete subsystems exercise rather than define the architecture. | [A14](architecture.md#a14) | P. |
| A14.2 | 171 | Responsiveness verification. | [A14](architecture.md#a14) | P. |
| A14.3 | 172 | Stress-handling verification. | [A14](architecture.md#a14) | P. |
| A14.4 | 173 | Message-routing verification. | [A14](architecture.md#a14) | P. |
| A14.5 | 174 | Ownership-boundary verification. | [A14](architecture.md#a14) | P. |
| A14.6 | 175 | Orchestration/execution interaction verification. | [A14](architecture.md#a14) | P. |
| A14.7 | 177 | A probe may be deleted; its composition is not Raven's boundary. | [A14](architecture.md#a14) | P. |
| A14.8 | 177 | Architectural conclusions remain subject to real-hardware validation. | [A14](architecture.md#a14), [decision process](architecture.md#governance) | P. |
| A14.9 | 179 | Axioms guide repository interpretation; layers express the stable roles. | [End of axioms](architecture.md#a14), [purpose](architecture.md#scope) | P. |

## Role definitions, responsibilities, and constraints

| ID | Baseline lines | Substantive claim | Preserved in | Outcome |
|---|---|---|---|---|
| ACT.D1 | 185-187 | Activity is a high-level orchestration/FSM-like layer. | [Activity definition](architecture.md#role-activity) | P. |
| ACT.D2 | 188 | Boot/idle/manual/autonomous/recovery/fault modes and transition ownership. | [Activity definition](architecture.md#role-activity) | P/C: the list remains an example, not a vehicle-specific restriction. |
| ACT.R1 | 190-192 | Subscribe to system events and react to State. | [ACT-01](architecture.md#role-activity) | P. |
| ACT.R2 | 193 | Select the services needed by a mode. | [ACT-02](architecture.md#role-activity) | P: activating a capability is not equated with constructing an RTOS task. |
| ACT.R3 | 194 | Directed commands during mode transitions. | [ACT-03](architecture.md#role-activity) | P. |
| ACT.R4 | 195 | Shared-state snapshots for orchestration decisions. | [ACT-04](architecture.md#role-activity) | P. |
| ACT.R5 | 196 | Own a FreeRTOS task and inbound queue; decisions run there. | [ACT-05](architecture.md#role-activity), [A06](architecture.md#a06), [INV-06](architecture.md#invariants) | M: owner context and input remain mandatory; FreeRTOS is a backend. |
| ACT.N1 | 198-200 | No direct hardware/peripheral API access. | [ACT-06](architecture.md#role-activity) | P. |
| ACT.N2 | 201 | No direct shared-state mutation; writes go through Controller. | [ACT-07](architecture.md#role-activity) | P/U: reconciled with line 134; publishing own state through the State API is allowed, bypassing it is not. |
| ACT.N3 | 202 | No blocking I/O. | [ACT-08](architecture.md#role-activity) | P: not weakened to allow arbitrary bounded I/O in Activity. |
| ACT.N4 | 203 | No business logic in an ESP-IDF callback. | [ACT-09](architecture.md#role-activity) | P/M: applies to platform callbacks on any backend. |
| SVC.D1 | 207-209 | Service is an asynchronous subsystem worker for ongoing/reactive work. | [Service definition](architecture.md#role-service), [SVC-01](architecture.md#role-service) | P. |
| SVC.D2 | 211 | Encapsulate a hardware subsystem or software stack; motors/sensors/Wi-Fi/telemetry/light/audio are examples. | [Service definition](architecture.md#role-service), [A14](architecture.md#a14) | P/C: the domain list does not restrict coherent subsystem responsibility. |
| SVC.R1 | 213-215 | Continuous servicing in its own FreeRTOS task. | [SVC-01](architecture.md#role-service), [A06](architecture.md#a06), [INV-06](architecture.md#invariants) | M: an ongoing owner lifecycle without requiring a busy loop or a particular RTOS. |
| SVC.R2 | 216 | Read hardware/external stacks and publish shared knowledge in State. | [SVC-02](architecture.md#role-service) | P. |
| SVC.R3 | 217 | Broadcast events about significant facts/transitions. | [SVC-03](architecture.md#role-service), [DEL-01](architecture.md#role-delivery) | P: Journal is not declared equivalent to behavioral publication. |
| SVC.R4 | 218 | Receive and execute directed commands from an inbound queue. | [SVC-04](architecture.md#role-service) | P. |
| SVC.N1 | 220-222 | No system-wide mode decisions. | [SVC-06](architecture.md#role-service) | P. |
| SVC.N2 | 223 | No access to another Service's internals. | [SVC-07](architecture.md#role-service), [dependencies](architecture.md#dependencies) | P. |
| SVC.N3 | 224 | Shared mutable data is not scattered through Service fields; it belongs in Controller. | [SVC-08](architecture.md#role-service), [STATE-04](architecture.md#role-state) | P: a concrete Service's public method does not replace an independent State contract. |
| STATE.D1 | 228-230 | Thread-safe canonical stores, the single source of truth for shared knowledge. | [State definition](architecture.md#role-state) | P. |
| STATE.D2 | 232 | One authoritative answer about battery/Wi-Fi/pose, not scattered Service fields. | [State definition](architecture.md#role-state), [A10](architecture.md#a10) | P: all three examples remain. |
| STATE.R1 | 234-236 | A thread-safe write API for services. | [STATE-01](architecture.md#role-state) | P: authorized write owners are explicit. |
| STATE.R2 | 237 | Thread-safe snapshots/read API for Activity and Service. | [STATE-02](architecture.md#role-state) | P. |
| STATE.R3 | 238 | Mutexes/atomics protect internal data. | [STATE-03](architecture.md#role-state) | P/M: equivalent synchronization mechanisms are permitted. |
| STATE.N1 | 240-242 | No business logic. | [STATE-05](architecture.md#role-state) | P: storage-integrity checks do not authorize choosing domain behavior. |
| STATE.N2 | 243 | No events/commands. | [STATE-06](architecture.md#role-state) | P. |
| STATE.N3 | 244 | No FreeRTOS task. | [STATE-07](architecture.md#role-state), [INV-06](architecture.md#invariants) | P/M: no autonomous executor on any backend. |
| ROOT.D1 | 248-250 | One composition/bootstrap root constructs and wires objects. | [Coordinator definition](architecture.md#role-coordinator), [ROOT-09](architecture.md#role-coordinator) | P: internal construction may be delegated; graph ownership remains defined. |
| ROOT.R1 | 252-254 | Initialize the ESP-IDF event loop. | [ROOT-01](architecture.md#role-coordinator) | M: the root initializes the platform through its backend; an ESP-specific loop is not imposed on a host. |
| ROOT.R2 | 255 | Construct and own Controller lifetimes. | [ROOT-02](architecture.md#role-coordinator) | P. |
| ROOT.R3 | 256 | Construct Services with injected State/controller references. | [ROOT-03](architecture.md#role-coordinator) | P. |
| ROOT.R4 | 257 | Construct Activities with State/service references. | [ROOT-04](architecture.md#role-coordinator) | P: dependencies are declared contracts, not foreign handlers. |
| ROOT.R5 | 258 | Register all event subscriptions. | [ROOT-05](architecture.md#role-coordinator), [DEL-01](architecture.md#role-delivery) | P/M: a component may hold the declaration; the root ensures complete wiring. |
| ROOT.R6 | 259 | Start tasks in the correct order. | [ROOT-06](architecture.md#role-coordinator) | P/M: owner/adapter ordering is preserved regardless of backend. |
| ROOT.R7 | 260 | Graceful shutdown. | [ROOT-07](architecture.md#role-coordinator) | P: remains mandatory; closing a TCP session does not count as full shutdown. |
| ROOT.N1 | 262-264 | No operational logic; wiring/startup only. | [ROOT-08](architecture.md#role-coordinator) | P. |
| ROOT.N2 | 265 | One Coordinator from app_main, not independent calls from multiple locations. | [ROOT-09](architecture.md#role-coordinator) | P/M: one root per instance; the backend selects the entry point. |
| DEL.D1 | 269-271 | Event-driven glue between layers. | [Delivery definition](architecture.md#role-delivery) | P. |
| DEL.D2 | 272 | Implementation on esp_event_loop. | [Delivery definition](architecture.md#role-delivery), [ROOT-01](architecture.md#role-coordinator) | M: SDK callbacks/loops are a platform boundary; the delivery role is portable. |
| DEL.R1 | 274-276 | Typed broadcast to all registered subscribers. | [DEL-01](architecture.md#role-delivery), [DEL-04](architecture.md#role-delivery) | P: all recipients are accounted for; partial admission is not presented as full success. |
| DEL.R2 | 277 | Directed commands through a typed command event or direct target queue. | [DEL-02](architecture.md#role-delivery), [A05](architecture.md#a05) | P: a declared addressed input with full admission is allowed, arbitrary bypass is not. |
| DEL.R3 | 277 | Reference to event-model.md. | [Interaction](architecture.md#interaction), [delivery](architecture.md#role-delivery) | C: the normative meaning is stated independently; the baseline's historical link is not edited. |
| DEL.R4 | 278 | Producer/consumer decoupling permits adding subsystems without changing their behavior. | [DEL-03](architecture.md#role-delivery), [dependencies](architecture.md#dependencies) | P/U: wiring changes are permitted and consistent with Coordinator; producers need not know consumer internals. |
| DEL.N1 | 280-282 | Do not share large mutable structures through messages; use State. | [DEL-05](architecture.md#role-delivery), [A08](architecture.md#a08) | P. |
| DEL.N2 | 283 | No business logic in callbacks; forward to the owner's queue and return promptly. | [DEL-06](architecture.md#role-delivery), [A09](architecture.md#a09) | P. |

## Invariants and interaction diagram

| ID | Baseline lines | Substantive claim | Preserved in | Outcome |
|---|---|---|---|---|
| I00 | 287-289 | The rules are mandatory and protect coherent growth. | [Invariants](architecture.md#invariants), [decision process](architecture.md#governance) | P: mandatory until an explicit revision is accepted. |
| I01.1 | 291 | Shared State is the single source of truth. | [INV-01](architecture.md#invariants) | P. |
| I01.2 | 292 | No private authoritative copies of shared knowledge; it lives in Controller. | [INV-01](architecture.md#invariants), [A10](architecture.md#a10) | U: snapshots are not prohibited; the ban on competing mutable authorities is consistent with lines 195/237. |
| I02.1 | 294 | Events are facts; commands are directed intent. | [INV-02](architecture.md#invariants), [interaction](architecture.md#interaction) | P. |
| I02.2 | 295 | Obstacle/battery/Wi-Fi are fact examples; stop/start are intent examples; do not conflate them. | [INV-02](architecture.md#invariants), [admission/execution](architecture.md#admission-execution) | P/C: the distinction remains without requiring specific subsystems. |
| I03.1 | 297 | Activity orchestrates; Service performs. | [INV-03](architecture.md#invariants) | P. |
| I03.2 | 298 | Activity does not drive hardware; Service does not select system-wide modes. | [INV-03](architecture.md#invariants) | P. |
| I04.1 | 300 | Event callbacks remain thin. | [INV-04](architecture.md#invariants) | P. |
| I04.2 | 301 | The SDK loop task is foreign; forward nontrivial work to the owner's input. | [INV-04](architecture.md#invariants), [A09](architecture.md#a09) | P/M: the rule applies to callbacks on any backend. |
| I05.1 | 303 | Business logic belongs in the owner task, not an event handler. | [INV-05](architecture.md#invariants) | P/M: owner context, not a mandatory RTOS task. |
| I05.2 | 304 | Rationale: priority inversion, event-loop stack overflow, and reentrancy. | [INV-05](architecture.md#invariants), [owner execution](architecture.md#owner-execution) | U: owner context alone does not prove the absence of every priority inversion or stack overflow; those properties need separate verification. The prohibition on foreign execution is unchanged. |
| I06.1 | 306 | Not every class/subsystem requires an RTOS task. | [INV-06](architecture.md#invariants) | P. |
| I06.2 | 307 | Tasks have memory/scheduling costs; reasons include independent execution, I/O, sampling, and queue servicing. | [INV-06](architecture.md#invariants) | P: all reasons and both costs remain. |
| I06.3 | 307 | Controller does not require a task. | [INV-06](architecture.md#invariants), [STATE-07](architecture.md#role-state) | P. |
| I07.1 | 309 | A clean foundation precedes premature framework complexity. | [INV-07](architecture.md#invariants) | P. |
| I07.2 | 310 | Do not introduce generic DI, runtime service registries, or heavy templates for hypothetical flexibility; use explicit wiring. | [INV-07](architecture.md#invariants), [dependencies](architecture.md#dependencies) | P: this addresses framework/service-location machinery, not a prohibition on domain fact registries. |
| G01 | 314-320 | Activity: FSM/orchestration, its own task, snapshots/commands/events. | [Activity](architecture.md#role-activity), [diagram](architecture.md#role-interactions) | P/M: owner context instead of a mandatory FreeRTOS primitive. |
| G02 | 321-323 | Activity → Service, directed commands. | [Diagram](architecture.md#role-interactions), [ACT-03](architecture.md#role-activity) | P: through admission, not a direct handler call. |
| G03 | 321-323 | Activity subscribes to events. | [Diagram](architecture.md#role-interactions), [ACT-01](architecture.md#role-activity), [DEL-01](architecture.md#role-delivery) | P. |
| G04 | 324-328 | A Service worker writes State, publishes events, and receives commands. | [Diagram](architecture.md#role-interactions), [SVC-01/02/03/04](architecture.md#role-service) | P. |
| G05 | 324-328 | Event Bus/esp_event_loop: broadcast facts and thin callbacks. | [Diagram](architecture.md#role-interactions), [delivery](architecture.md#role-delivery) | P/M: no SDK implementation is imposed; both responsibilities remain. |
| G06 | 329-335 | Service reads/writes State; State is thread-safe, task-free, and canonical. | [Diagram](architecture.md#role-interactions), [State](architecture.md#role-state) | P. |
| G07 | 338 | All objects are constructed and wired by Coordinator/app_main. | [Diagram](architecture.md#role-interactions), [Coordinator](architecture.md#role-coordinator) | P/M: the entry point is platform-specific; each instance has one root. |

## Clarifications checked separately

1. **Context and task.** Requirements at lines 196/215/319/326 remain as ownership
   of input and sequential execution. A concrete FreeRTOS task is a mechanism:
   this follows the original A01/A06/A07 and invariant 6, and does not authorize
   running domain work in a foreign context.
2. **State and copies.** The prohibition at line 292 is not turned into a ban on
   snapshots required by lines 195/237. An independent State contract, controlled
   writes, and the absence of behavior, events, and an autonomous task remain mandatory.
3. **Broadcast and admission.** A publication still targets all registered
   subscribers. Explicit per-recipient admission describes rejection under bounded
   resources; it does not permit silently excluding recipients or treating Journal
   as a replacement for event interaction.
4. **Addressed input.** Direct target-queue delivery comes from original line 277.
   It does not permit bypassing Dispatcher/routing: the input must be an established
   route with the same semantic and admission checks. Public method visibility
   is not architectural permission.
5. **Callbacks.** An adapter performs bounded technical work at the external
   boundary. Activity/Service domain logic remains exclusively with its owner.
6. **Owner-context rationale.** The rule at line 303 is preserved. Line 304 is
   not used as unconditional proof that every priority or stack problem is absent:
   those guarantees do not follow from context selection alone.
7. **Graceful shutdown.** Line 260 remains mandatory in ROOT-07. Compliance is
   not inferred from the existence of close for a single session.

## General communication extensions

These extensions replace none of the original requirements and do not restrict
the architecture to the composition of a particular application.

| Extension | Normative location | Connection to original principles |
|---|---|---|
| Independent instances, local routing, external device-oriented language | COM-01 | A01, A03, A04, A05, Coordinator |
| Ingress does not wait for domain execution; receipts use an outbound contract | COM-02, admission/execution | A04, A09, delivery and Service roles |
| Egress and serialized writes with independent I/O placement | COM-03 | A06, A07, invariant 6 |
| Lifecycle ownership and generation protection for State commits/responses | COM-04/05 | A08, A10, Controller, Coordinator |
| Separate timing stages; timeout is not NACK | COM-06 | A09, A11, A12, Service responsibility |
| Semantic retries, duplicates, readiness, and keepalive belong to the contract owner | COM-07 | A09, A11, Service |
| Remote State as observation; identity/readiness/link/outcome are distinct | COM-08 | A03, A10, Controller |
| Explicit capacity/overload/recovery and fate of accepted work | COM-09 | A05, A08, A14 |
| Device count is a resource profile, not a definition of roles | COM-10 | A01, A06, A14, invariants 6/7 |
| Ownership of finite-operation outcomes; executor deadlines are separate from observers | SVC-05/09, ACT-10, admission/execution | A09, A11, A12 |
| Critical as a safety-critical contract; Latest as permitted value replacement | Delivery policies | A03, A05, A08, A09 |

## Stage result

All 14 axioms, definitions of the five original roles, their responsibilities and
prohibitions, the seven invariants, and the original diagram's connections have
counterparts in the normative document. Historical statements about branches,
class names, and domain composition are distinguished from their architectural properties.

No statement was found whose transfer requires removing or weakening an architectural
principle. Mechanism and explanatory clarifications are listed explicitly above.
This table does not resolve implementation compliance: an independent Architecture
Compliance Review and any code corrections are separate subsequent stages.
