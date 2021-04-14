idf_component_register(SRCS "ota_util.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::app_update
                    REQUIRES idf::esp_https_ota
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/
                    EMBED_FILES update_root.pem update_public_key.pem)
project(otaupdate)
