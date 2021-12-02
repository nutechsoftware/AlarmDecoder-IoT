if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "4.1")
set(EXTRA_EMBED1 "sendgrid_root.pem")
set(EXTRA_EMBED2 "twilio_root.pem")
endif()
idf_component_register(SRCS "twilio.cpp" "lib/fmt/src/format.cc"
                    REQUIRES idf::esp-tls
                    REQUIRES idf::json
                    REQUIRES idf::esp_http_client
                    REQUIRES idf::alarmdecoder-api
                    INCLUDE_DIRS . ../../main/ lib/fmt/include
                    EMBED_TXTFILES ${EXTRA_EMBED1}
                                   ${EXTRA_EMBED2})
project(twilio)
