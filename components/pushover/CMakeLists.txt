idf_component_register(SRCS "pushover.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/
                    EMBED_TXTFILES ../../certs/pushover_root.pem)
project(pushover)
