idf_component_register(SRCS "twilio.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    INCLUDE_DIRS . ../../main/
                    EMBED_FILES twilio_root.pem)
project(twilio CXX)
