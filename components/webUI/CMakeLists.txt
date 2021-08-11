idf_component_register(SRCS "webUI.cpp" "TinyTemplateEngine.cpp" "TinyTemplateEngineFileReader.cpp"
                    REQUIRES idf::alarmdecoder-api
                    REQUIRES idf::esp-tls
                    REQUIRES idf::json
                    REQUIRES idf::esp_http_server
                    INCLUDE_DIRS . ../../main/)
project(webUI)
