idf_component_register(SRCS "ser2sock.cpp"
                    REQUIRES idf::esp-tls
                    REQUIRES idf::json
                    REQUIRES idf::esp_http_client
                    REQUIRES idf::alarmdecoder-api
                    INCLUDE_DIRS . ../../main/)
project(ser2sock)
