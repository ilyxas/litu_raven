# LITU - Raven
R&amp;D platform combining on-device LLM intelligence in Swift (MLX) with a strictly structured FreeRTOS-based ESP32 runtime, exploring end-to-end mobile-to-embedded system design.

LITU - Raven is an under-development split system composed of:

- **Raven (ESP esp-idf)** — the embodied executor operating in the physical environment
- **LITU (iOS swift 6)** — the higher-level interpretation and world-modelling layer

The ESP side is responsible for sensing, movement, local reaction, and producing telemetry and state from contact with the world.

The iOS side is responsible for building a usable representation of that world, maintaining higher-level state, visualising system behaviour, and hosting local semantic reasoning.

The local LLM is intended as a semantic reasoning layer, not as a replacement for deterministic execution.

The project is still evolving. Some current components are experimental or scaffolding layers used to validate architecture, protocol boundaries, local LLM integration, and visual runtime behaviour while the broader system direction is being shaped.


For more details, see:
- [`ESP/raven/docs/`](ESP/raven/docs/)
- [`IOS/docs/`](IOS/docs/)

## Estimated token statistics

- `IOS/litu/Engine`: **19,644** tokens
- `ESP/raven/components`: **14,182** tokens
