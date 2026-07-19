cmake_minimum_required(VERSION 3.21)

project(ota_gateway LANGUAGES C CXX)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/../experiments/wakaama
    ${CMAKE_CURRENT_BINARY_DIR}/wakaama
)

set(
    WAKAAMA_CLIENT_COMMON_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../experiments/wakaama/examples/client/common
)

add_library(ota_gateway_objects STATIC)

target_sources(
    ota_gateway_objects
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
    ota_gateway_objects
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    ota_gateway_objects
    PUBLIC
        wakaama_static
)

add_executable(
    ota_gateway_smoke
    tests/gateway_smoke.cpp
    src/wakaama_hooks.c
    include/wakaama_hooks.h
)

target_link_libraries(
    ota_gateway_smoke
    PRIVATE
        ota_gateway_objects
)

add_executable(
    ota_gateway
    src/main.cpp
    src/gateway_app.cpp
    src/wakaama_hooks.c
    include/gateway_app.hpp
    include/wakaama_hooks.h
)

target_link_libraries(
    ota_gateway
    PRIVATE
        ota_gateway_objects
)
