# Raven — Architecture

This document defines the role boundaries for the layers that make up the Raven runtime, and the invariants that every contributor must respect.

---

## Layers

### Activities

Activities are the high-level orchestration / FSM-like behaviour layer.  
They represent the **mode** the vehicle is currently operating in (boot, idle, manual, autonomous, recovery, fault, …) and own the decision flow for transitioning between those modes.

**Responsibilities**

- Subscribe to system events and react to state changes.
- Decide which services should be active for a given mode.
- Post directed commands to services when a mode transition requires action.
- Read shared state snapshots to make orchestration decisions.
- Own a FreeRTOS task and an inbound queue; all decision logic runs inside that task.

**Must not**

- Directly access hardware drivers or low-level peripheral APIs.
- Mutate shared state directly (that is the controller's job).
- Perform blocking I/O.
- Execute business logic inside an ESP-IDF event callback.

---

### Services

Services are **asynchronous subsystem workers** responsible for ongoing or reactive operational work.

They encapsulate interaction with a single hardware subsystem or software stack (motor driver, IMU, rangefinder, Wi-Fi, telemetry, lighting, audio, …).

**Responsibilities**

- Run continuously inside their own FreeRTOS task.
- Read from hardware / external stacks and write results into shared state.
- Publish broadcast events when something noteworthy happens (sensor threshold crossed, connection status changed, …).
- Receive and execute directed commands posted to their inbound queue.

**Must not**

- Make system-wide mode decisions (that is the activity's job).
- Access another service's internals directly.
- Scatter mutable state across their own fields when that state is consumed by other layers — it belongs in a controller.

---

### Controllers / Shared State

Controllers are **thread-safe canonical state stores** — the single source of truth for any piece of system-wide data.

There is one definitive answer to "what is the current battery level?", "is Wi-Fi connected?", "what is the vehicle's last known pose?" and it lives in a controller, not scattered across service member variables.

**Responsibilities**

- Expose a thread-safe write API for services to push updates.
- Expose a thread-safe snapshot / read API for activities and services to query the current state.
- Protect internal data with mutexes or atomics as appropriate.

**Must not**

- Contain business logic.
- Post events or send commands.
- Own a FreeRTOS task.

---

### Coordinator

The Coordinator is the **root composition and bootstrap layer** — the only place in the system where all other objects are constructed and wired together.

**Responsibilities**

- Initialise the ESP-IDF event loop.
- Construct and own controller instances.
- Construct and own service instances, injecting controller references.
- Construct and own activity instances, injecting controller and service references.
- Register all event subscriptions.
- Start the FreeRTOS tasks for services and activities in the correct order.
- Provide a graceful shutdown path.

**Must not**

- Contain operational logic (it only wires and starts, it does not decide).
- Be called from multiple places; there is exactly one Coordinator, created in `app_main`.

---

### Event Bus

The Event Bus is the **event-driven glue** between layers.  
It is implemented on top of the ESP-IDF system event loop (`esp_event_loop`).

**Responsibilities**

- Deliver typed, broadcast events to all registered subscribers.
- Carry directed commands from one component to another (wrapped as typed command events or posted directly to a target queue — see `docs/event-model.md`).
- Decouple producers from consumers so that new subsystems can be added without modifying existing code.

**Must not**

- Be used to share large mutable data structures — use shared state for that.
- Execute business logic inside a callback — callbacks must forward to the owner task's queue and return immediately.

---

## Architectural Invariants

The following rules are non-negotiable. They protect the coherence of the system as it grows.

1. **Shared state is the single source of truth.**  
   No service or activity maintains its own private copy of data that other layers need. If the data matters to more than one component, it lives in a controller.

2. **Events communicate facts; commands communicate directed intent.**  
   An event says "this happened" (obstacle detected, battery low, Wi-Fi connected). A command says "you specifically should do this" (stop motors, start telemetry stream). Do not conflate the two.

3. **Activities orchestrate; services perform subsystem work.**  
   An activity never drives hardware directly. A service never decides system-wide mode transitions.

4. **Event callbacks must remain thin.**  
   The ESP-IDF event loop delivers callbacks on its own internal task. Any non-trivial work must be forwarded to the receiving component's own queue and processed in that component's task context.

5. **Business logic executes in owner task context, not inside event handlers.**  
   This prevents priority inversion, stack overflows on the event-loop task, and subtle re-entrancy bugs.

6. **Not every class or subsystem needs its own FreeRTOS task.**  
   Tasks add memory overhead and scheduling complexity. A FreeRTOS task is justified when a component genuinely needs concurrent, independent execution (blocking I/O, periodic sampling, queue-driven event processing). Controllers, for example, never need their own task.

7. **Prefer a clean foundation over premature framework complexity.**  
   Avoid generic DI containers, runtime service registries, or heavy template abstractions at this stage. Clarity and explicit wiring are more valuable than flexibility that is not yet needed.

---

## Layer Interaction Summary

```
┌──────────────────────────────────────────────────────────┐
│                        Activity                          │
│  (FSM / orchestration — runs in its own FreeRTOS task)   │
│  reads state snapshots · posts commands · reacts to events│
└───────────────┬─────────────────────────┬────────────────┘
                │ directed commands        │ event subscriptions
                ▼                          ▼
┌──────────────────────────┐   ┌──────────────────────────┐
│         Service          │   │        Event Bus          │
│  (subsystem worker task) │──▶│  (esp_event_loop glue)    │
│  writes state · publishes│   │  broadcast facts          │
│  events · handles cmds   │   │  thin callbacks only      │
└───────────┬──────────────┘   └──────────────────────────┘
            │ writes / reads
            ▼
┌──────────────────────────┐
│  Controller / Shared State│
│  (thread-safe, no task)   │
│  single source of truth   │
└──────────────────────────┘

All objects constructed and wired by: Coordinator (app_main)
```

---

*See also: [`runtime-model.md`](runtime-model.md) · [`event-model.md`](event-model.md)*
