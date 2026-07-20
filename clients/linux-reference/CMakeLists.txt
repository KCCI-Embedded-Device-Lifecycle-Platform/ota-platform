cmake_minimum_required(VERSION 3.21)

project(ota_linux_reference_client LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WAKAAMA_MODE_CLIENT ON)
set(WAKAAMA_MODE_SERVER OFF)
set(WAKAAMA_MODE_BOOTSTRAP_SERVER OFF)

set(WAKAAMA_UNIT_TESTS OFF)
set(WAKAAMA_ENABLE_EXAMPLES OFF)

set(WAKAAMA_TRANSPORT POSIX_UDP)
set(WAKAAMA_PLATFORM POSIX)
set(WAKAAMA_CLI ON)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../experiments/wakaama
    ${CMAKE_CURRENT_BINARY_DIR}/wakaama
)

set(
    WAKAAMA_CLIENT_COMMON_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../experiments/wakaama/examples/client/common
)

add_library(ota_linux_reference_client_objects STATIC)

target_sources(
    ota_linux_reference_client_objects
    PRIVATE
        src/object_bms.c
        include/object_bms.h
        src/object_firmware.c
        include/object_firmware.h
        include/standard_objects.h
        ${WAKAAMA_CLIENT_COMMON_DIR}/object_security.c
        ${WAKAAMA_CLIENT_COMMON_DIR}/object_server.c
        ${WAKAAMA_CLIENT_COMMON_DIR}/object_device.c
)

target_include_directories(
    ota_linux_reference_client_objects
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    ota_linux_reference_client_objects
    PUBLIC
        wakaama_static
)

add_executable(
    ota_linux_reference_client_smoke
    tests/reference_client_smoke.cpp
    src/wakaama_hooks.c
    include/wakaama_hooks.h
)

target_link_libraries(
    ota_linux_reference_client_smoke
    PRIVATE
        ota_linux_reference_client_objects
)

add_executable(
    ota_linux_reference_client
    src/main.cpp
    src/reference_client_app.cpp
    src/wakaama_hooks.c
    include/reference_client_app.hpp
    include/wakaama_hooks.h
)

target_link_libraries(
    ota_linux_reference_client
    PRIVATE
        ota_linux_reference_client_objects
)
