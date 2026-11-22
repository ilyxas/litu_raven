idf_component_register(
    SRCS "src/raven_activities.cpp"
         "src/dual_arm_activity.cpp"
    INCLUDE_DIRS "include"
    REQUIRES raven_contracts raven_state raven_core
)
