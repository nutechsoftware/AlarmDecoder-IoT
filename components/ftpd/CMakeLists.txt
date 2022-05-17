idf_component_register(SRCS "ftpd.cpp"
                    REQUIRES idf::esp-tls
                    REQUIRES idf::json
                    REQUIRES idf::esp_http_client
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::esp_http_server
                    INCLUDE_DIRS . ../../main/)
project(ftpd)
