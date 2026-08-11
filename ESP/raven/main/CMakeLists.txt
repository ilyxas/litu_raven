idf_component_register(
    SRCS "app_main.cpp"
         "flight_controller_bootstrap.cpp"
    INCLUDE_DIRS "."
    REQUIRES raven_core raven_events raven_state raven_activities raven_services raven_platform raven_gateways
)
