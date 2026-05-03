idf_component_register(
    SRCS "src/raven_activities.cpp"
         "src/navigation_activity.cpp"
    INCLUDE_DIRS "include"
    REQUIRES raven_core raven_events raven_state raven_services
)
