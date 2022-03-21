cmake_minimum_required(VERSION 3.15)
project(adq-rapid)
add_executable(${PROJECT_NAME} src/main.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(ADQ REQUIRED libadq)

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
    add_compile_definitions(LINUX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wswitch -Wswitch-enum -Werror=overflow)
elseif(WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4 /WX)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    ${GLFW3_CFLAGS}
    ${ADQ_CFLAGS}
)

target_link_libraries(${PROJECT_NAME}
    GL
    pthread
    ${GLFW3_STATIC_LIBRARIES}
    ${ADQ_LIBRARIES}
)

