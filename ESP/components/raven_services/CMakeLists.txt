idf_component_register(SRCS "src/peer_contracts.cpp" "src/peer_state.cpp" "src/peer_service.cpp" "src/diagnostics_service.cpp" INCLUDE_DIRS "include" REQUIRES raven_comms raven_diagnostics)
