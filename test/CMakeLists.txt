# This file is intended to be included as a target by  top-level CMakeLists and
# does not work on its own.  Build with: `cmake --build -t test`

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.15)
project(test
    DESCRIPTION "sigscape test suite"
    LANGUAGES CXX C)

add_executable(${PROJECT_NAME})

# Fetch CppUTest
include(FetchContent)
FetchContent_Declare(
    CppUTest
    GIT_REPOSITORY https://github.com/cpputest/cpputest
    GIT_TAG 34f8c4548ec55cf1ae5f28824cdd816b60e618cc
)

# Causes conflicts
set(CPPUTEST_MEM_LEAK_DETECTION_DISABLED ON CACHE INTERNAL "")

FetchContent_MakeAvailable(CppUTest)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data_processing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/digitizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/file_watcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data_types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mock/generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mock/adqapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mock/digitizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mock/system_manager.cpp
    tgenerator.cpp
    tdata_processing.cpp
    tthread_safe_queue.cpp
    tmessage_thread.cpp
    tfile_watcher.cpp
    tdigitizer.cpp
    main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../simple_fft
    ${fmt_SOURCE_DIR}/include)

if (UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wswitch -Werror=overflow)
elseif (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _USE_MATH_DEFINES
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    MOCK_ADQAPI)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    fmt
    CppUTest)

# Add a custom command do run the test suite as a CMake target.
add_custom_target(run_tests
    COMMAND $<TARGET_FILE:${PROJECT_NAME}> -c -v -s
    USES_TERMINAL)

add_dependencies(run_tests ${PROJECT_NAME})