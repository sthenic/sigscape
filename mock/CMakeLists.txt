set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.15)
project(adq
    DESCRIPTION "A mockup of the ADQAPI shared library"
    LANGUAGES CXX C)

add_library(${PROJECT_NAME} SHARED)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sine_generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pulse_generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mock_control_unit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mock_digitizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mock_system_manager.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${json_SOURCE_DIR}/include)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ADQAPI_INTERNAL
    SIGSCAPE_REVISION=\"${SIGSCAPE_REVISION}\")

if (UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wswitch -Werror=overflow)
elseif (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
        NOMINMAX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4)
endif()
