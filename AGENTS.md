# LITU / Raven: agent entry point

Before substantive work, read [Handoff/README.md](Handoff/README.md). It preserves collaboration context and a map of the current documentation. Then check the current Git state and the user's request: the handoff supplies context, not new assignments, and does not replace the architectural specification.

## Working with Ilya

- Ilya often thinks aloud and conveys intent through examples. Extract the system property from an example; do not turn its domain, specific classes, or mechanisms into requirements without a reason.
- Make justified engineering decisions independently within the current task. State significant assumptions briefly. Ask only about questions that materially change the goal or contract; do not seek agreement on every technical detail.
- Challenge proposals with concrete reasoning. Do not replace architectural discussion with repeated explanations of basic RTOS and hardware limitations; raise them where they affect the contract under discussion.
- Distinguish user requirements, your proposals, and implemented behavior. Do not present an assumption as an agreed decision.
- Communicate with Ilya in Russian, directly and naturally. Take responsibility for the entire authorized task: implementation, necessary verification, and a clear result.

## Repository language

- English is the required language for all repository-authored text: documentation, READMEs, ADRs, diagrams, agent instructions, code comments, and diagnostic or user-facing messages. Use English for commit messages and pull request descriptions as well.
- The conversation may be in Russian. The conversation's language does not determine the language of repository artifacts.
- Preserve identifiers, commands, protocol literals, links, measurements, and immutable source material exactly where their contracts require it. A translation must not silently change an architectural rule or a recorded result.
- A language rule does not expand task authorization: when only documentation changes are authorized, do not edit source code or tests to perform language cleanup.

## Current R&D boundaries

- The general Raven model matters more than any particular demo. Arms, an oven, navigation, and file uploads illustrate system properties; MSP and virtual actuators are implementation examples.
- Do not conflate message delivery, protocol ACK/NACK, and operation execution. General contracts are defined in the [Raven architectural specification](ESP/docs/architecture.md).
- Do not change iOS code in this direction without a separate task. Legacy ESP-side compatibility has been removed; a future iOS protocol will be planned separately.
- The direction's base branch is `dev_rnd`; working branches and PRs target it. Do not merge this direction into `main` without a new explicit user decision.
- Before writing, check the current branch and other people's changes. When multiple chats share a checkout, do not switch it or interfere with their work without necessity and an agreed basis.

## Documentation and verification

- The normative architectural specification is [ESP/docs/architecture.md](ESP/docs/architecture.md). Do not adjust it to fit current code: an implementation deviation does not change the rule. The technical communication implementation is described separately in [communication-architecture.md](ESP/docs/communication-architecture.md).
- The [architecture baseline](ESP/docs/architecture-baseline-02dd622.md) is an immutable, byte-identical copy of `02dd622:ESP/raven/docs/architecture.md`. Do not edit, reformat, or update it. Provenance, checksum, and the comparison with the current specification are in [architecture-coverage.md](ESP/docs/architecture-coverage.md).
- The technical map and verification commands are in [ESP/README.md](ESP/README.md) and [validation.md](ESP/docs/validation.md); the current stage's decisions are in [ADR-003](ESP/docs/adr/0003-device-communication.md), and the delivery/execution contract is in [ADR-001](ESP/docs/adr/0001-delivery-and-execution.md).
- Verify guarantees and behavior, not merely that a test matches the implementation. Distinguish host verification, ESP verification, TCP loopback, and a real iOS client.
- When a significant contract or stage status changes, update the relevant existing documentation and, when needed, the handoff. Do not create a separate report for every run. Reading files alone does not require updates.
- Do not claim a build, test, flash, or PR action succeeded without supporting evidence. Distinguish information from an earlier report from your own verification.
