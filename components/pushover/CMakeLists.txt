if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "4.1")
set(EXTRA_EMBED "pushover_root.pem")
endif()
idf_component_register(SRCS "pushover.cpp"
                    REQUIRES idf::esp-tls
                    REQUIRES idf::json
                    REQUIRES idf::esp_http_client
                    REQUIRES idf::alarmdecoder-api
                    INCLUDE_DIRS . ../../main/
                    EMBED_TXTFILES ${EXTRA_EMBED})
project(pushover)
