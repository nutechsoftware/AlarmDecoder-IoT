idf_component_register(SRCS "twilio.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/
                    EMBED_TXTFILES ../../certs/twilio_root.pem ../../certs/sendgrid_root.pem)
project(twilio)
