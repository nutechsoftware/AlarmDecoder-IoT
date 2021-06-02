if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "4.1")
set(EXTRA_EMBED1 "../../certs/sendgrid_root.pem")
set(EXTRA_EMBED2 "../../certs/twilio_root.pem")
endif()
idf_component_register(SRCS "twilio.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/
                    EMBED_TXTFILES ${EXTRA_EMBED1}
                                   ${EXTRA_EMBED2})
project(twilio)
