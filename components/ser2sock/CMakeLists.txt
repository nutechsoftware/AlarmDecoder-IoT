idf_component_register(SRCS "ser2sock.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/)
project(ser2sock)
