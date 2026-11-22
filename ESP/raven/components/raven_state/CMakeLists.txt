idf_component_register(
    SRCS "src/raven_state.cpp"
         "src/flight_controller_state.cpp"
         "src/operation_state.cpp"
    INCLUDE_DIRS "include"
    REQUIRES  raven_core
)
