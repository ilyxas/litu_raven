cmake_minimum_required(VERSION 3.16)
project(raven_host_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)
set(COMPONENTS "${CMAKE_CURRENT_LIST_DIR}/../../components")
add_executable(runtime_test runtime_test.cpp ../runtime_self_test.cpp
    ${COMPONENTS}/raven_core/src/inbox.cpp
    ${COMPONENTS}/raven_core/src/dispatcher.cpp
    ${COMPONENTS}/raven_core/src/executor.cpp
    ${COMPONENTS}/raven_state/src/operation_state.cpp
    ${COMPONENTS}/raven_platform/src/sim_arm_driver.cpp
    ${COMPONENTS}/raven_services/src/arm_service.cpp
    ${COMPONENTS}/raven_activities/src/dual_arm_activity.cpp
    ${COMPONENTS}/raven_gateways/src/command_protocol.cpp
    ${COMPONENTS}/raven_gateways/src/frame_reader.cpp
    ${COMPONENTS}/raven_gateways/src/serial_control_protocol.cpp)
target_include_directories(runtime_test PRIVATE ..)
foreach(component raven_core raven_contracts raven_state raven_platform raven_services raven_activities raven_gateways)
    target_include_directories(runtime_test PRIVATE ${COMPONENTS}/${component}/include)
endforeach()
target_link_libraries(runtime_test PRIVATE Threads::Threads)
if(MSVC)
    target_compile_options(runtime_test PRIVATE /W4 /WX)
else()
    target_compile_options(runtime_test PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME runtime COMMAND runtime_test)
# The existing POSIX serial protocol tests use ssize_t.
if(NOT MSVC)
    foreach(test msp_codec msp_client)
        add_executable(${test}_test ${test}_test.cpp ${COMPONENTS}/raven_gateways/src/msp_codec.cpp)
        target_include_directories(${test}_test PRIVATE ${COMPONENTS}/raven_gateways/include ${COMPONENTS}/raven_platform/include)
        target_compile_options(${test}_test PRIVATE -Wall -Wextra -Werror)
        add_test(NAME ${test} COMMAND ${test}_test)
    endforeach()
    target_sources(msp_client_test PRIVATE ${COMPONENTS}/raven_gateways/src/msp_client.cpp ${COMPONENTS}/raven_gateways/src/betaflight_msp_client.cpp)
endif()
