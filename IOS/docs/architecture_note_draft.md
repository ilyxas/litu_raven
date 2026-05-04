# LITU iOS — Architecture Note (Draft)

This note explains the intended architectural role of the iOS side of the project. It is written to prevent the repository from being misread as a collection of unrelated experimental screens. The current application contains temporary surfaces and fast-moving prototypes, but they exist in service of a coherent direction.

## Current reality

The iOS application is still under active construction. Some current surfaces may look unrelated at first glance: a local LLM console, a RealityKit-based penalty scene, TCP networking controls, and other lightweight experiments. This is a consequence of development order, not of architectural incoherence.

At this stage, the iOS app has primarily functioned as a sandbox for:
- integrating and tuning on-device LLM execution,
- testing protocol boundaries with the embedded runtime,
- validating visual/interactive runtime ideas,
- and moving quickly toward a demonstrable end-to-end system.

The current center of gravity is the local LLM integration layer. Other surfaces are supporting probes, validation harnesses, or early scaffolding for the larger system.

## Project-level role split

The overall system is built around two major roles:

### ESP32 side

The ESP runtime is the embodied explorer operating in the physical environment.

It is responsible for:
- sensing,
- movement,
- local execution,
- real-time interaction with the environment,
- and producing raw telemetry and state from contact with the world.

The ESP side is not just a passive endpoint. It is the field-side executor and investigator in the unknown.

### iOS side

The iOS side is not merely a control client or a companion app. Its long-term role is to serve as the higher-level world-construction and interpretation layer.

It is intended to:
- visualise the environment and the explorer state,
- maintain a structured world model,
- build and update a graph of places, transitions, and costs,
- accumulate semantic information over that graph,
- hold and interpret the state of the ESP explorer,
- and host the on-device LLM reasoning layer that operates over semantic abstractions rather than raw control.

In short:

**ESP lives in the world.  
iOS builds and interprets the world model.**

## Intended direction of the iOS architecture

The iOS layer is moving toward four tightly related responsibilities.

### 1. Visual world construction

RealityKit is not present for game-like novelty. It is being explored as the basis for visual world representation.

The intent is that a human should not be forced to understand the system through raw telemetry alone. Instead, the iOS side should provide:
- a spatial view of what is happening,
- where the explorer is,
- what surrounds it,
- what the recent motion implies,
- and how the local environment should be understood.

The exact visual form may evolve. The architectural point is stable: visualisation is a core interpretation surface, not decoration.

### 2. Graph-based world state

The iOS side is intended to maintain a graph of traversable or relevant space:
- nodes,
- edges,
- transition costs,
- and accumulated facts tied to locations or paths.

Base costs may come from simple deterministic measures such as time or traversal effort from node X to node Y. But the graph is not purely geometric. It is expected to carry richer context as the system evolves, for example:
- water,
- drift,
- unstable terrain,
- steep incline,
- difficult passage,
- or other experience-derived properties.

This turns the iOS side into a keeper of structured world knowledge, not just a renderer of telemetry.

### 3. State aggregation and interpretation

The iOS layer is intended to hold the higher-level state of the explorer as reported by ESP:
- battery,
- graph position,
- observed conditions,
- traversal history,
- and other numerically reported state.

This state is not important only as raw numbers. The iOS side exists to transform raw state into a form that can be:
- visualised,
- reasoned over,
- related to a world model,
- and used to influence future decisions.

### 4. Semantic mediation for local LLM reasoning

One of the most important roles of the iOS side is to implement the intermediate layer between raw numeric/system data and semantic representations suitable for local LLM reasoning.

The LLM is not intended to replace deterministic planning or low-level control. Its role is different.

The iOS-side LLM is intended to:
- observe semanticised system state,
- monitor trajectory dynamics,
- interpret patterns that are difficult to encode as simple rules,
- recognise tension, instability, quietness, risk, or drift in system behaviour,
- and contribute meaning under uncertainty.

This is the key distinction:

**The LLM is not the direct pathfinder.  
The LLM is the semantic advisor in uncertainty.**

## Role of the LLM in the intended system

The LLM should not be treated as a generic chat endpoint or as a magical planner for everything.

Its intended use is narrower and more architectural:
- it receives semantic abstractions, not raw motor control,
- it analyses evolving context, not only isolated prompts,
- it operates over trajectory meaning and situational dynamics,
- and it influences decision-making indirectly.

More specifically, the LLM is expected to help by reweighting the graph rather than replacing it.

For example:
- deterministic logic may compute the shortest path,
- but the LLM may detect that a region, route, or recent behaviour pattern carries latent risk,
- and therefore increase the effective cost of transitions in that part of the graph,
- which the deterministic planner will then take into account.

This preserves a strong architectural separation:
- deterministic algorithms remain responsible for actual path computation,
- while the LLM contributes semantic judgement where uncertainty, incomplete formalisation, or early pattern recognition matter most.

## How the current iOS surfaces should be interpreted

The present screens are not the architecture itself. They are development surfaces that expose different aspects of the intended system.

### Local LLM surface

The so-called chat surface is not primarily a product chat feature. It is an operational console for on-device LLM integration.

Its purpose is to let development feel and control:
- model loading,
- model selection,
- generation parameters,
- session lifecycle,
- history-based restart,
- token throughput,
- time-to-first-token,
- memory pressure,
- and the overall behaviour of local inference as a real subsystem.

It exists because the LLM layer is one of the future centers of gravity of the iOS side, and it needed to be sharpened early.

### RealityKit / penalty-style surfaces

These are not important because of the specific game theme. They exist as visual and interaction testbeds.

Their purpose is to:
- stabilise and understand RealityKit integration,
- exercise contract-driven LLM calls inside a live loop,
- test structured request/response patterns,
- and explore how visual runtime surfaces may eventually represent world state and system behaviour.

The domain theme is incidental. The architectural role is what matters.

### TCP networking surface

The TCP view is not a finished networking subsystem. It is a simplified protocol and transport workbench.

It exists to:
- exercise ingress and egress,
- inspect frames,
- send commands,
- observe telemetry,
- validate decoder behaviour,
- and make the mobile-to-embedded boundary visible during development.

Its purpose is transparency and fast iteration, not production completeness.

## Why the project can currently look uneven

The current repository state reflects development sequencing:
- the ESP side received heavy architectural focus,
- the iOS LLM integration was pulled in and refined early,
- and the surrounding iOS application has so far served as a rapid sandbox for experiments, validation, and demo-oriented assembly.

Because of this, some older descriptions or maps may no longer represent the true center of the codebase. The right way to read the iOS side is not as a set of unrelated features, but as an emerging system built around:
- world construction,
- semantic state interpretation,
- protocol bridging,
- visualisation,
- and local LLM reasoning.

## Architectural summary

The intended shape of the system is this:

- **ESP** is the embodied explorer in the unknown physical world.
- **iOS** constructs and maintains the interpretable world model around that explorer.
- **iOS deterministic logic** manages graph, state, visualisation, and route computation.
- **iOS LLM logic** acts as a semantic advisor under uncertainty, influencing interpretation and transition weighting rather than replacing core deterministic planning.

In condensed form:

**ESP senses and moves.  
iOS models and interprets.  
The iOS LLM helps the system understand when the world means more than raw numbers alone can express.**

## Status note

This note describes the intended direction, not a claim that every part of the current iOS code already fully reflects that direction. Some surfaces are still scaffolding. Some names may still reflect older experiments. Some abstractions are still forming.

The purpose of this note is to preserve the architectural line while the implementation continues to move.
