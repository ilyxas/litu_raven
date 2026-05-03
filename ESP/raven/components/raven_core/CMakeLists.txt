idf_component_register(
    SRCS "src/raven_core.cpp"
         "src/base_task.cpp"
         "src/SystemMonitorTask.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos
)
