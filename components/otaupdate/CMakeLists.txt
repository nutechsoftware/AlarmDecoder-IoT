set(EXTRA_EMBED "ota_update_server_root.pem")
idf_component_register(SRCS "ota_util.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::app_update
                    REQUIRES idf::esp_https_ota
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/
                    EMBED_TXTFILES firmware_signature_public_key.pem ${EXTRA_EMBED})
project(otaupdate)