cmake_minimum_required(VERSION 3.15)
project(adq-rapid VERSION 1.0)
add_executable(${PROJECT_NAME} src/main.cpp)

if (UNIX)
    if (APPLE)
      find_library(COCOA Cocoa REQUIRED)
      target_compile_definitions(${PROJECT_NAME} PRIVATE SIMULATION_ONLY)
    else()
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(ADQ REQUIRED libadq)
    endif()
elseif(WIN32)
    message(WARNING "Not implemented for Windows yet.")
endif()

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

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
target_sources(${PROJECT_NAME} PRIVATE
    lib/GL/gl3w.c
    implot/implot.cpp
    implot/implot_items.cpp
    implot/implot_demo.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    src/simulator.cpp
    src/data_processing.cpp
    src/simulated_digitizer.cpp
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    implot
    imgui
    simple_fft
    lib
    include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GL3W
)

if (UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LINUX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wswitch -Wswitch-enum -Werror=overflow)
elseif(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _USE_MATH_DEFINES)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4 /WX)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    ${ADQ_CFLAGS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GL
    pthread
    glfw
    ${COCOA}
    ${ADQ_LIBRARIES}
)

