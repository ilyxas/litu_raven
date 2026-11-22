idf_component_register(
    SRCS "src/raven_core.cpp"
         "src/base_task.cpp"
         "src/SystemMonitorTask.cpp"
         "src/inbox.cpp"
         "src/dispatcher.cpp"
         "src/executor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES pthread freertos
)
