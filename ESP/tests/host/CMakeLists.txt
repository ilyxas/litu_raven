cmake_minimum_required(VERSION 3.16)
project(raven_host_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(RAVEN_SANITIZER "" CACHE STRING "Optional Linux sanitizer: address or thread")
if(RAVEN_SANITIZER)
    if(NOT RAVEN_SANITIZER MATCHES "^(address|thread)$")
        message(FATAL_ERROR "RAVEN_SANITIZER must be address or thread")
    endif()
    add_compile_options(-fno-omit-frame-pointer -g -fsanitize=${RAVEN_SANITIZER})
    add_link_options(-fsanitize=${RAVEN_SANITIZER})
    if(RAVEN_SANITIZER STREQUAL "address")
        add_compile_options(-fsanitize=undefined)
        add_link_options(-fsanitize=undefined)
    endif()
endif()
enable_testing()
find_package(Threads REQUIRED)
set(COMPONENTS "${CMAKE_CURRENT_LIST_DIR}/../../components")
add_library(raven_portable OBJECT
    ${COMPONENTS}/raven_core/src/inbox.cpp
    ${COMPONENTS}/raven_core/src/dispatcher.cpp
    ${COMPONENTS}/raven_core/src/executor.cpp
    ${COMPONENTS}/raven_icd/src/device_protocol.cpp
    ${COMPONENTS}/raven_comms/src/length_framing.cpp
    ${COMPONENTS}/raven_comms/src/session.cpp
    ${COMPONENTS}/raven_comms/src/tx_component.cpp
    ${COMPONENTS}/raven_diagnostics/src/journal.cpp
    ${COMPONENTS}/raven_diagnostics/src/log_capture.cpp
    ${COMPONENTS}/raven_services/src/peer_contracts.cpp
    ${COMPONENTS}/raven_services/src/peer_state.cpp
    ${COMPONENTS}/raven_services/src/peer_service.cpp
    ${COMPONENTS}/raven_services/src/diagnostics_service.cpp)
foreach(component raven_core raven_icd raven_comms raven_diagnostics raven_services)
    target_include_directories(raven_portable PUBLIC ${COMPONENTS}/${component}/include)
endforeach()
target_link_libraries(raven_portable PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(raven_portable PUBLIC /W4 /WX)
else()
    target_compile_options(raven_portable PUBLIC -Wall -Wextra -Werror)
endif()
add_executable(communication_test communication_test.cpp)
target_link_libraries(communication_test PRIVATE raven_portable)
add_test(NAME communication COMMAND communication_test)
add_executable(service_contract_test service_contract_test.cpp)
target_link_libraries(service_contract_test PRIVATE raven_portable)
add_test(NAME service_contracts COMMAND service_contract_test)
