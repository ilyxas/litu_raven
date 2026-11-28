# ADR-004: Explicit Behavioral Roles and Independent Peer State

Date: 2026-11-26. Status: accepted for the current R&D correction.

## Context

Component provides an execution contract but does not distinguish orchestration,
subsystem work, or adaptation. Empty Activity and Service bases make the intended
behavioral role visible in the type declaration without changing execution.

DiagnosticsService depended on PeerService for shared-state reads and called its
probe method, which posted directly into the service's Inbox. This coupled an observer
to a behavior owner instead of an independent State and declared command route.

The correction implements the existing specification: A02/A06/A11, SVC-07/08,
STATE-01/02/04, and DEL-02/04. It does not revise those principles or constitute the
full PR Architecture Compliance Review.

## Decision

- Add empty Activity and Service subclasses of Component in the portable core.
  PeerService and DiagnosticsService declare Service. TCP/BLE TX adapters remain
  Component; no production Activity is added solely to instantiate that role.
- Keep Executor/OwnerTask based on Component. Role declarations add no scheduling,
  queues, fields, runtime role switches, or automatic orchestration.
- Put shared peer observations and ProbeRequest/ProbeResult definitions in
  `peer_contracts.hpp`, independently of the executing service's header/source.
- Construct PeerState in the composition root. It owns shared-state synchronization,
  the immutable local descriptor, peer observations, and the stale-message counter.
  Composition retains ownership of bounded record/session storage.
- Inject mutable PeerState into PeerService and const PeerState into DiagnosticsService.
  The writer uses a scoped Write transaction; references obtained from it must not
  outlive that transaction. Readers receive copied snapshots.
- Keep session-validity projection in the snapshot contract: an inactive or previous
  generation is reported as disconnected immediately, without waiting for a service
  tick. This reads session state; it does not close a session or decide service behavior.
- Send manual probes through Dispatcher to a configured peer endpoint. Results continue
  to return through Dispatcher. Unbound, invalid, and full routes reject admission
  without executing the service.

## Alternatives and trade-offs

Renaming or wrapping PeerService's public methods would retain observer dependence
on the executing service. Copying its registry into DiagnosticsService would create
a second mutable authority. Both alternatives are rejected.

PeerState and peer contracts stay in the existing domain package; a new generic
state framework or ESP-IDF component is unnecessary for this correction. Write
transactions make synchronization explicit and preserve existing lock scope, at
the cost of passing the transaction through private service helpers.

The store still represents the current communication domain, including session keys
and validity. This change does not redesign the ICD/transport boundary or claim
that every remaining architectural constraint has been reviewed.

## Preserved behavior and resources

PeerState → Session → Inbox lock order and generation-checked commits are preserved.
Pending exchanges, retries, deadlines, and outcomes remain owned by PeerService.
The ICD, message payloads, admission/result distinction, BLE commands/output,
session lifecycle, queues, and task configuration are unchanged.

Empty role bases have Component's size. On ESP32-S3, the linked Runtime grows from
32 240 to 32 256 bytes (+16); Message remains 88 bytes and Inbox 1 520 bytes.
No tasks, queues, mutexes, or dynamic per-message allocations are added: the existing
peer-state mutex moves from PeerService into PeerState.

## Verification

The original 17 host communication scenarios remain. Three additional service-contract
scenarios check snapshot independence and close/reuse validity, concurrent snapshot
consistency, and DiagnosticsService operation through State and Dispatcher without
including or constructing PeerService. They distinguish admission from execution and
cover missing routes, full queues, and asynchronous probe results.

Current build/test evidence and the distinction from earlier hardware results are
recorded in [validation.md](../validation.md).
