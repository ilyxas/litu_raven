idf_component_register(
    SRCS "app_main.cpp"
         "gateway_bootstrap.cpp"
    PRIV_REQUIRES esp_wifi nvs_flash bt
    INCLUDE_DIRS "."
    REQUIRES raven_core raven_events raven_state raven_activities raven_services raven_platform raven_gateways esp_driver_gpio esp_adc
)
