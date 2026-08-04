add_library(
    ota_device_integration_kit_wakaama
    STATIC
        src/object_firmware.c
        include/object_firmware.h
)

target_include_directories(
    ota_device_integration_kit_wakaama
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    ota_device_integration_kit_wakaama
    PUBLIC
        ota_device_integration_kit
        wakaama_static
)

target_compile_features(
    ota_device_integration_kit_wakaama
    PUBLIC
        c_std_99
)