idf_component_register(
    SRCS "src/raven_platform.cpp"
         "src/EspUartPort.cpp"
         "src/TcpClientLink.cpp"
         "src/TcpSessionLink.cpp"
         "src/TcpServer.cpp"
         "src/sim_arm_driver.cpp"
         "src/owner_task.cpp"
         "src/tcp_io.cpp"
         "src/wifi_network.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer esp_wifi esp_netif nvs_flash raven_core raven_contracts esp_driver_uart lwip bt
)
if(CONFIG_RAVEN_BLE_ENABLED)
    target_sources(${COMPONENT_LIB} PRIVATE "src/ble_serial_server.cpp")
endif()
