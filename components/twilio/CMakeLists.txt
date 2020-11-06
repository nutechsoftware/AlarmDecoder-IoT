idf_component_register(SRCS "twilio.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    INCLUDE_DIRS . ../../main/
                    EMBED_TXTFILES twilio_root.pem sendgrid_root.pem)
project(twilio)
