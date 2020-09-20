idf_component_register(SRCS "stsdk_main.c"
                            "caps_carbonMonoxideDetector.c"
                            "caps_contactSensor.c"
                            "caps_momentary.c"
                            "caps_refresh.c"
                            "caps_securitySystem.c"
                            "caps_smokeDetector.c"
                            "caps_switch.c"
                            "caps_tamperAlert.c"
                    INCLUDE_DIRS .)
project(stsdk)
