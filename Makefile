#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := alarmdecoder_ad2iot_esp32

EXTRA_COMPONENT_DIRS := ${IDF_PATH}/../../iot-core

include $(IDF_PATH)/make/project.mk

