idf_component_register(
    SRCS "src/raven_services.cpp"
         "src/navigation_service.cpp"
         "src/pilot_input_service.cpp"
         "src/telemetry_service.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__cjson
    REQUIRES raven_core raven_events raven_state raven_gateways
)
