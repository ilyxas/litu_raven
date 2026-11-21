idf_component_register(
    SRCS "src/raven_gateways.cpp"
         "src/network_gateway.cpp"
         "src/network_client_gateway.cpp"
         "src/msp_codec.cpp"
         "src/msp_client.cpp"
         "src/betaflight_msp_client.cpp"
    INCLUDE_DIRS "include"
    REQUIRES raven_core raven_platform
)
