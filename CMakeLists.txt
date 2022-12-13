cmake_minimum_required(VERSION 3.15)
project(adq-rapid VERSION 1.0)
add_executable(${PROJECT_NAME})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NO_ADQAPI "Compile with the ADQAPI as a dependency. With this set to ON, only the simulator mode is available." OFF)

if (UNIX)
    if (APPLE)
      target_compile_definitions(${PROJECT_NAME} PRIVATE SIMULATION_ONLY)
      set(NO_ADQAPI ON)
    else()
      find_package(PkgConfig REQUIRED)
      find_package(OpenGL REQUIRED)
    endif()
elseif(WIN32)
    find_package(OpenGL REQUIRED)
endif()

find_package(Threads REQUIRED)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

include(FetchContent)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.3.6
)

# Disable examples etc.
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE INTERNAL "Generate installation target")
# TODO: Set GLFW_BUILD_SHARED_LIBRARY to something?

FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  gl3w
  GIT_REPOSITORY https://github.com/skaslev/gl3w
  GIT_TAG 5f8d7fd191ba22ff2b60c1106d7135bb9a335533 # Some commit
)
FetchContent_MakeAvailable(gl3w)

FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot
    GIT_TAG v0.14
)
FetchContent_MakeAvailable(implot)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG 9.1.0
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG v1.2.13
)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(
    png
    GIT_REPOSITORY https://github.com/glennrp/libpng
    GIT_TAG v1.6.39
)

set(SKIP_INSTALL_ALL ON)

set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
set(ZLIB_LIBRARIES zlibstatic)

set(PNG_BUILD_ZLIB ON CACHE BOOL "")
set(PNG_STATIC ON CACHE BOOL "")
set(PNG_SHARED OFF CACHE BOOL "")
set(PNG_EXECUTABLES OFF CACHE BOOL "")
set(PNG_TESTS OFF CACHE BOOL "")

FetchContent_MakeAvailable(png)

# It doesn't work to link directly to gl3w for some reason.
# Maybe something to do with https://github.com/skaslev/gl3w/issues/66
# However adding the generation target as a dependency and then
# including and building the generated sources works
# gl3w.c must be set as GENERATED to prevent cmake from complaining that
# the filed doesn't exist during generation
add_dependencies(${PROJECT_NAME} gl3w_gen)
set_property(SOURCE ${gl3w_BINARY_DIR}/src/gl3w.c PROPERTY GENERATED 1)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
target_sources(${PROJECT_NAME} PRIVATE
    # BINARY_DIR is required here since we want to use the generated code
    # from gl3w
    ${gl3w_BINARY_DIR}/src/gl3w.c
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    src/data_processing.cpp
    src/digitizer.cpp
    src/identification.cpp
    src/file_watcher.cpp
    src/ui.cpp
    src/marker.cpp
    src/format.cpp
    src/data_types.cpp
    src/window.cpp
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${implot_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${png_SOURCE_DIR}
    ${fmt_SOURCE_DIR}/include
    simple_fft
    ${gl3w_BINARY_DIR}/include
    include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GL3W
)

if(NO_ADQAPI)
    message(STATUS "Using NO_ADQAPI (no ADQAPI dependency).")
    target_compile_definitions(${PROJECT_NAME} PRIVATE NO_ADQAPI)
    target_sources(${PROJECT_NAME} PRIVATE
        src/mock_adqapi.cpp
        src/mock_digitizer.cpp
        src/generator.cpp)
endif()

if (UNIX)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        LINUX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wswitch -Werror=overflow)
elseif(WIN32)
    # TODO: Maybe this should be behind elsif(MSCV) instead?
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _USE_MATH_DEFINES
        IMGUI_USER_CONFIG="imgui_config.h")
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4) # FIXME: Once we had /WX too
endif()

# ADQAPI localization
# FIXME: Add option to build the API as a first-hand choice.
if (NOT NO_ADQAPI)
    if (UNIX)
        find_library(ADQAPI_LIBRARY
            NAMES libadq libadq.so
            HINTS
            "${CMAKE_SOURCE_DIR}/../../../source/.libs/"
            "${CMAKE_SOURCE_DIR}/../../../build/"
        )
    else()
        set (ADQAPI_LOCAL_PATH "") # FIXME:
        set (ADQAPI_INSTALL_PATH "$ENV{PROGRAMFILES}/SP Devices/ADQAPI_x64/")

        find_library(ADQAPI_LIBRARY
            NAMES ADQAPI.lib
            HINTS
            ${ADQAPI_LOCAL_PATH}/x64/Debug/
            ${ADQAPI_INSTALL_PATH}
        )

        find_file(ADQAPI_DLL
            NAMES ADQAPI.dll
            HINTS
            ${ADQAPI_LOCAL_PATH}/x64/Debug/
            ${ADQAPI_INSTALL_PATH}
        )

        if(NOT ADQAPI_DLL)
            message(FATAL_ERROR "ADQAPI.dll not found")
        endif()

        # FIXME: Copy the DLL somewhere?
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ADQAPI_DLL}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endif()

    find_path(ADQAPI_INCLUDE
        NAMES ADQAPI.h
        HINTS
        ${ADQAPI_LOCAL_PATH}/Release
        "${CMAKE_SOURCE_DIR}/../../../Release/"
        "${CMAKE_SOURCE_DIR}/../../../build/"
        ${ADQAPI_INSTALL_PATH}
    )
    target_include_directories(${PROJECT_NAME} PRIVATE ${ADQAPI_INCLUDE})

    message(STATUS "Using libadq/ADQAPI.lib: ${ADQAPI_LIBRARY}")
    message(STATUS "Using ADQAPI.h from ${ADQAPI_INCLUDE}/ADQAPI.h")
    if (ADQAPI_DLL)
        message(STATUS "Using ADQAPI.dll: ${ADQAPI_DLL}")
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    Threads::Threads
    glfw
    fmt
    png_static
    ${ADQAPI_LIBRARY}
)
