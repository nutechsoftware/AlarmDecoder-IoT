idf_component_register(SRCS "ad2mqtt.cpp"
                    REQUIRES idf::esp-tls
                    REQUIRES idf::json
                    REQUIRES idf::esp_http_client
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::mqtt
                    INCLUDE_DIRS . ../../main/)
project(ad2mqtt)
