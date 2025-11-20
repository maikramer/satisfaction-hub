# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(satisfaction-hub)

# Habilitar suporte a exceções C++ para todo o projeto
# Isso permite o uso de try/catch e exceções do nlohmann::json
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)