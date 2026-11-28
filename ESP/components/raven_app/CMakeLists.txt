idf_component_register(SRCS "src/bootstrap.cpp" INCLUDE_DIRS "include" REQUIRES raven_platform raven_services)
# MC main selects BLE before Kconfig expansion. Node never pulls the BT stack.
idf_component_optional_requires(PRIVATE raven_ble)
if(CONFIG_RAVEN_TCP_BOARD_PROBE)
    target_sources(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../tests/tcp_board_probe.cpp")
endif()
