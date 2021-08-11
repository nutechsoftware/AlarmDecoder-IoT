idf_component_register(SRCS "ser2sock.cpp"
                    REQUIRES idf::esp-tls
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/)
project(ser2sock)
