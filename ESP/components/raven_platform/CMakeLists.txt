idf_component_register(SRCS "src/owner_task.cpp" "src/wifi_network.cpp" "src/tcp_network.cpp" "src/system_log.cpp"
    INCLUDE_DIRS "include" REQUIRES raven_comms raven_diagnostics esp_timer esp_wifi esp_event esp_netif nvs_flash lwip)
