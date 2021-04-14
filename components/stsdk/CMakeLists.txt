idf_component_register(SRCS "stsdk_main.cpp"
                            "caps_battery.c"
                            "caps_powerSource.c"
                            "caps_button.c"
                            "caps_carbonMonoxideDetector.c"
                            "caps_contactSensor.c"
                            "caps_momentary.c"
                            "caps_refresh.c"
                            "caps_securitySystem.c"
                            "caps_smokeDetector.c"
                            "caps_switch.c"
                            "caps_tamperAlert.c"
                    EMBED_FILES "device_info.json"
                                "onboarding_config.json"
                    REQUIRES idf::otaupdate
                    INCLUDE_DIRS . ../../main/ $ENV{STDK_CORE_PATH}/src/include/)
project(stsdk)
