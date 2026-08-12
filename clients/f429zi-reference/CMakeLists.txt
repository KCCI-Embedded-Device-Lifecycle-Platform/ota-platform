cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME f429zi-reference)
set(F429ZI_APP_VERSION "0.1.0" CACHE STRING "Reference application version")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

set(
    OTA_DEVICE_INTEGRATION_KIT_BUILD_TESTS
    OFF
    CACHE BOOL
    ""
    FORCE
)

set(WAKAAMA_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(WAKAAMA_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

set(WAKAAMA_MODE_CLIENT ON CACHE BOOL "" FORCE)
set(WAKAAMA_MODE_SERVER OFF CACHE BOOL "" FORCE)
set(WAKAAMA_MODE_BOOTSTRAP_SERVER OFF CACHE BOOL "" FORCE)

set(WAKAAMA_CLIENT_INITIATED_BOOTSTRAP OFF CACHE BOOL "" FORCE)
set(WAKAAMA_CLIENT_LWM2M_V_1_0 OFF CACHE BOOL "" FORCE)

set(WAKAAMA_DATA_TLV ON CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_JSON OFF CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_SENML_JSON OFF CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_SENML_CBOR OFF CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_OLD_CONTENT_FORMAT OFF CACHE BOOL "" FORCE)

set(WAKAAMA_TRANSPORT NONE CACHE STRING "" FORCE)
set(WAKAAMA_PLATFORM NONE CACHE STRING "" FORCE)
set(WAKAAMA_CLI OFF CACHE BOOL "" FORCE)

set(WAKAAMA_LOG_LEVEL INFO CACHE STRING "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../experiments/wakaama
    ${CMAKE_CURRENT_BINARY_DIR}/wakaama
    EXCLUDE_FROM_ALL
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../device-integration-kit
    ${CMAKE_CURRENT_BINARY_DIR}/device-integration-kit
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../device-integration-kit/adapters/wakaama
    ${CMAKE_CURRENT_BINARY_DIR}/device-integration-kit-wakaama
)

set(
    WAKAAMA_CLIENT_COMMON_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../experiments/wakaama/examples/client/common
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/esp01_uart.c
    Core/Src/esp01_modem.c
    Core/Src/esp01_transport.c
    Core/Src/esp01_coap_download_transport.c
    Core/Src/f429zi_firmware_backend.c
    Core/Src/f429zi_lwm2m_client.c
    Core/Src/wakaama_mapping_stm32.c
    ${WAKAAMA_CLIENT_COMMON_DIR}/object_security.c
    ${WAKAAMA_CLIENT_COMMON_DIR}/object_server.c
    ${WAKAAMA_CLIENT_COMMON_DIR}/object_device.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${WAKAAMA_CLIENT_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../experiments/wakaama/coap/er-coap-13
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    F429ZI_APP_VERSION="${F429ZI_APP_VERSION}"
    USER_VECT_TAB_ADDRESS
    VECT_TAB_OFFSET=0x00020000U
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    ota_device_integration_kit
    ota_device_integration_kit_wakaama
    ota_device_integration_kit_wakaama_client
    wakaama_static
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.bin
    COMMENT "Generating application BIN file"
)
