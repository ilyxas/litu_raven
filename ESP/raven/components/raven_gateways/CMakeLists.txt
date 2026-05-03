idf_component_register(
    SRCS "src/raven_gateways.cpp"
         "src/network_gateway.cpp"
         "src/network_client_gateway.cpp"
    INCLUDE_DIRS "include"
    REQUIRES raven_core raven_platform
)
