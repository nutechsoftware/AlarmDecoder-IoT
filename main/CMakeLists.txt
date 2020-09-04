idf_component_register(SRCS "alarmdecoder_stsdk_app_esp32.cpp"
                            "device_control.c"
                            "iot_cli_cmd.c"
                            "iot_uart_cli.c"
                            "ota_util.c"
                            "caps_switch.c"
                            "caps_tamperAlert.c"
                            "caps_smokeDetector.c"
                            "caps_securitySystem.c"
                            "caps_momentary.c"
                            "caps_contactSensor.c"
                            "caps_carbonMonoxideDetector.c"
                    EMBED_FILES "device_info.json"
                                "onboarding_config.json"
                    REQUIRES idf::alarmdecoder-api
                    )

set(STDK_IOT_CORE_USE_DEFINED_CONFIG "y")

set(STDK_LINK_LIBRARY
        __idf_libsodium
        __idf_json
   )

set(STDK_INCLUDE_PATH
        "$ENV{IDF_PATH}/components/freertos/include/freertos"
        "$ENV{IDF_PATH}/components/nvs_flash/include"
        "$ENV{IDF_PATH}/components/spi_flash/include"
        "$ENV{IDF_PATH}/components/bootloader_support/include"
    )

add_subdirectory($ENV{STDK_CORE_PATH} iotcore)
target_link_libraries(${COMPONENT_LIB} PUBLIC iotcore)
