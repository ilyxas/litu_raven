idf_component_register(
    SRCS "src/raven_platform.cpp"
         "src/EspUartPort.cpp"
         "src/TcpClientLink.cpp"
         "src/TcpSessionLink.cpp"
         "src/TcpServer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart lwip
)
