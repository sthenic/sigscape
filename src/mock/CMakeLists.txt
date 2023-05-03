set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.15)
project(adq
    DESCRIPTION "A mockup of the ADQAPI shared library"
    LANGUAGES CXX C)

add_library(${PROJECT_NAME} SHARED)

set_property(TARGET ${PROJECT_NAME} PROPERTY CMAKE_CXX_STANDARD 17)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/control_unit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digitizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/system_manager.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ADQAPI_INTERNAL)
