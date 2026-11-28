idf_component_register(SRCS "src/ble_serial_server.cpp" "src/ble_console.cpp" INCLUDE_DIRS "include"
    REQUIRES raven_platform raven_services bt)
