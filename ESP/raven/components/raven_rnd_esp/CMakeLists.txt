idf_component_register(SRCS "src/runtime.cpp" INCLUDE_DIRS "include"
    REQUIRES raven_rnd raven_core raven_events esp_wifi esp_netif nvs_flash esp_timer lwip)
