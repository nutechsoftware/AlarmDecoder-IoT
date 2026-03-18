idf_component_register(SRCS "usdupdate.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::app_update
                    REQUIRES idf::esp-tls
                    REQUIRES idf::esp_https_ota
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/)
project(usdupdate)
