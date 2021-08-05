idf_component_register(SRCS "ad2mqtt.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    REQUIRES idf::mqtt
                    INCLUDE_DIRS . ../../main/)
project(ad2mqtt)
