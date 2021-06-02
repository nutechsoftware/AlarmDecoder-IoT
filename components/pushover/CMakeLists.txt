if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "4.1")
set(EXTRA_EMBED "../../certs/pushover_root.pem")
endif()
idf_component_register(SRCS "pushover.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    INCLUDE_DIRS . ../../main/
                    EMBED_TXTFILES ${EXTRA_EMBED})
project(pushover)
