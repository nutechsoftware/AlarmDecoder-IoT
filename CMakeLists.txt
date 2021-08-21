# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Problem with IPv6 being disabled and these components.
set(EXCLUDE_COMPONENTS "coap" "asio")

project(alarmdecoder_stsdk_esp32)

