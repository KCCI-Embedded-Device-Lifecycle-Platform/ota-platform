add_library(
    ota_device_integration_kit
    STATIC
        src/firmware_update_service.c
        include/firmware_update_backend.h
        include/firmware_update_service.h
)

target_include_directories(
    ota_device_integration_kit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    ota_device_integration_kit
    PUBLIC
        c_std_99
)

option(
    OTA_DEVICE_INTEGRATION_KIT_BUILD_TESTS
    "Build Device Integration Kit tests"
    ON
)

if(OTA_DEVICE_INTEGRATION_KIT_BUILD_TESTS)
    add_executable(
        ota_device_integration_kit_test
        tests/firmware_update_service_test.c
    )

    target_link_libraries(
        ota_device_integration_kit_test
        PRIVATE
            ota_device_integration_kit
    )
endif()