# ADR-001: Separate delivery receipts from owner execution

Status: Implemented on `dev_rnd`'s prototype branch; not a migration of `main`.

Historical prototype record. Its delivery/execution distinction remains in force;
the arm demo and wire protocol below were removed by [ADR-003](0003-device-communication.md).
See [current architecture](../architecture.md) for the implemented layout and contracts.

## Context

TCP ingress must promptly acknowledge delivery without waiting for a device to
complete an operation. A continuous target stream, an independent finite goal,
a callback and a critical command have different delivery requirements.
Activities trust the service's operation contract; services own device-specific
timeouts. The current BaseTask conflates a single FIFO with the execution backend
and allocates every payload on the heap.

## Decision

- Add a platform-independent execution substrate with fixed-value envelopes,
  immutable subscriptions and a synchronous `try-deliver` result. ACK means the
  recipient inbox accepted a copy, **not** that execution started or succeeded.
- Ordered commands use an eight-entry FIFO. Explicit latest-value and critical
  subscriptions reserve separate slots. Only messages whose contracts allow
  replacement use those slots. Critical IDs are local subscription metadata,
  never an untrusted priority field in a packet.
- Services own operation state and deadlines. Activities poll child status.
  External queries use a bounded 16-result history, with `Unknown` after eviction
  or reboot. Active scenarios reserve fixed-capacity result handles before child
  dispatch, so an unobserved result cannot be evicted by unrelated operations.
  They remember terminal outcomes and release handles after observation or when
  their wait ends. Exhaustion rejects the scenario before child dispatch.
  No network result-receipt ACK or Activity-owned child deadline is required.
- A critical cut is an idempotent request implemented by each owner. Its inbox
  sequence is a barrier against older queued goals. Subsequent goals may run;
  the architecture imposes no global latch, Reset command or reboot requirement.
  Activity additionally cancels its allocated child-ID range to cover children
  dispatched concurrently with the cut. That cancellation cannot stop another
  caller's newer goal; it is domain logic, not an inbox-wide priority rule.
- One task per arm, one activity task, independent sensors, TCP ingress and
  telemetry. All device decisions run in the owning service. Callbacks only
  copy envelopes into that owner's inbox. Task notifications only wake it.
- The executor checks critical messages, due ticks and ordinary/latest work on
  every step. Ticks use monotonic, phase-based scheduling, coalescing missed
  ticks. Device deadlines are absolute times owned by services.
- Broadcast delivery is not atomic. ACK/NACK contains accepted/rejected endpoint
  masks; a partial NACK does not undo deliveries. Pair orchestration also does
  not roll back already admitted children or cut unrelated goals on failure.
- The command listener remains serviceable while a session is connected. The
  newest connection replaces the previous controller and resets only transport
  parsing. Accepted operations continue in their services. TCP keepalive also
  cleans up dead sockets, but recovery with a new client does not wait for it.

## Alternatives and consequences

A single FIFO plus send-to-front cannot admit critical messages into a full
queue. An unbounded priority queue moves the problem to heap exhaustion. Reserved
coalescing slots provide bounded admission for the small declared critical set;
they are unsuitable for arbitrary distinct critical requests requiring a separate
execution for every occurrence.

A reliable completion event bus would couple operation retention to observers.
Polling retained state keeps execution independent of network consumers, at the
cost of a documented retention window. Callers must not confuse `Unknown` with
failure or automatically replay an evicted request.
Internal scenario waits need stronger retention than this diagnostic history.
Four explicit handles per state bound the memory cost and reject excess waits;
enlarging the history alone would only postpone eviction. Handles retain state,
not execution, and cannot extend or replace a service's deadline.

Waiting for keepalive alone would delay reconnect behind probe expiry. Explicit
session replacement provides immediate admission of a new controller; the trade-off
is that a second connection takes over even from a healthy first controller.
Multi-client arbitration is outside the current single-controller prototype.

The old BaseTask and FC adapters remain available as the alternate boot mode.
New code does not inherit them: its behavior can be tested with synthetic time
using exactly the same classes that run on FreeRTOS. This prototype does not
claim hard-real-time latency or process-crash persistence. Critical delivery does
not preempt an already executing C++ function; the supplied owners remain
responsive by their tested state-machine/driver contracts.

## Verification

The deterministic suite tests queue saturation, critical repetition, deadline
progress under ingress, retargeting, callback correlation, status retention,
partial delivery, fragmented frames, child-result eviction and phase-shifted
retargeting. Boot tests additionally use real FreeRTOS tasks and lwIP sockets
while a telemetry consumer stops reading temporarily and while a previous command
client remains connected without closing its session.
