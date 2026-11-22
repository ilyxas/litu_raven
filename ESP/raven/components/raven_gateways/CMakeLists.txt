idf_component_register(
    SRCS "src/raven_gateways.cpp"
         "src/network_gateway.cpp"
         "src/network_client_gateway.cpp"
         "src/msp_codec.cpp"
         "src/msp_client.cpp"
         "src/betaflight_msp_client.cpp"
         "src/command_protocol.cpp"
         "src/frame_reader.cpp"
         "src/command_server.cpp"
         "src/tcp_frame_writer.cpp"
         "src/telemetry_publisher.cpp"
         "src/serial_control_protocol.cpp"
    INCLUDE_DIRS "include"
    REQUIRES  raven_contracts raven_state raven_core raven_platform
)
if(CONFIG_RAVEN_BLE_ENABLED)
    target_sources(${COMPONENT_LIB} PRIVATE "src/ble_control_gateway.cpp")
endif()
