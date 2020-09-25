idf_component_register(SRCS "ad2_cli_cmd.cpp" "alarmdecoder_main.cpp"
                            "ad2_uart_cli.cpp"
                            "device_control.cpp"
                            "iot_cli_cmd.cpp"
                            "iot_uart_cli.cpp"
                            "ota_util.ppc"
                            "ad2_utils.cpp"
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::twilio
		    REQUIRES idf::stsdk
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
