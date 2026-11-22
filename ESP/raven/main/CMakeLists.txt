set(app_sources "app_main.cpp" "flight_controller_bootstrap.cpp" "demo_bootstrap.cpp")
if(CONFIG_RAVEN_RND_SELF_TEST)
    list(APPEND app_sources "../tests/runtime_self_test.cpp" "../tests/board_self_test.cpp")
endif()
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." "../tests"
    REQUIRES raven_core raven_events raven_state raven_activities raven_services raven_platform raven_gateways)
