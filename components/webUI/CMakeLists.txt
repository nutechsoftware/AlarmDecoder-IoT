idf_component_register(SRCS "webUI.cpp" "TinyTemplateEngine.cpp" "TinyTemplateEngineFileReader.cpp"
                    REQUIRES idf::nvs_flash
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::json
                    REQUIRES idf::esp_http_server
                    REQUIRES idf::fatfs
                    INCLUDE_DIRS . ../../main/)
project(webUI)
