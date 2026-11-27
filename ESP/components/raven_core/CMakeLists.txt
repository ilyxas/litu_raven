idf_component_register(SRCS "src/inbox.cpp" "src/dispatcher.cpp" "src/executor.cpp" INCLUDE_DIRS "include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
