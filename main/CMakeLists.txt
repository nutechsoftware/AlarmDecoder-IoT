idf_component_register(SRCS "ad2_utils.cpp" "alarmdecoder_main.cpp"
                            "device_control.cpp"
                            "ad2_cli_cmd.cpp"
                            "ad2_uart_cli.cpp"
                    REQUIRES idf::esp-tls
                    REQUIRES idf::json
                    REQUIRES idf::fatfs
		    REQUIRES idf::stsdk
                    REQUIRES idf::esp_http_client
                    REQUIRES idf::spiffs
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::otaupdate
                    REQUIRES idf::pushover
                    REQUIRES idf::twilio
                    REQUIRES idf::ser2sock
                    REQUIRES idf::webUI
                    REQUIRES idf::ad2mqtt
                    REQUIRES idf::ftpd
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

# Fetch SimpleIni lib and as as include path.
#  Todo: Only if not already downloaded.
include(FetchContent) # once in the project to include the module
FetchContent_Declare(simpleini GIT_REPOSITORY https://github.com/brofield/simpleini.git)
FetchContent_MakeAvailable(simpleini)
include_directories(${simpleini_SOURCE_DIR})

if(CONFIG_STDK_IOT_CORE)
        add_subdirectory($ENV{STDK_CORE_PATH} iotcore)
        target_link_libraries(${COMPONENT_LIB} PUBLIC iotcore)
endif()
