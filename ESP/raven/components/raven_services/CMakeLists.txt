idf_component_register(
    SRCS "src/raven_services.cpp"
         "src/flight_controller_service.cpp"
         "src/arm_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES raven_contracts raven_core raven_events raven_state raven_gateways
)
