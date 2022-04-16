cmake_minimum_required(VERSION 3.15)
project(adq-rapid VERSION 1.0)
add_executable(${PROJECT_NAME})

option(WITH_ADQAPI "Compile without the ADQAPI as a dependency. With this set to OFF, only the simulator mode is available." ON)

if (UNIX)
    if (APPLE)
      target_compile_definitions(${PROJECT_NAME} PRIVATE SIMULATION_ONLY)
    #   set(WITH_ADQAPI OFF) FIXME: Would this work?
    else()
      find_package(PkgConfig REQUIRED)
      find_package(OpenGL REQUIRED)
      pkg_check_modules(ADQ REQUIRED libadq)
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
    GIT_TAG v0.13
)
FetchContent_MakeAvailable(implot)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.87
)
FetchContent_MakeAvailable(imgui)

# It doesn't work to link directly to gl3w for some reason.
# Maybe something to do with https://github.com/skaslev/gl3w/issues/66
# However adding the generation target as a dependency and then
# including and building the generated sources works
# gl3w.c must be set as GENERATED to prevent cmake from complaining that
# the filed doesn't exist during generation
add_dependencies(${PROJECT_NAME} gl3w_gen)
set_property(SOURCE ${gl3w_BINARY_DIR}/src/gl3w.c PROPERTY GENERATED 1)

if(NOT WITH_ADQAPI)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        SIMULATION_ONLY)
    target_sources(${PROJECT_NAME} PRIVATE
        src/mock_adqapi.cpp
        src/mock_digitizer.cpp)
endif()

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
    src/generator.cpp
    src/data_processing.cpp
    src/digitizer.cpp
    src/file_watcher.cpp
    src/ui.cpp
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${implot_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    simple_fft
    ${gl3w_BINARY_DIR}/include
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
    # TODO: Maybe this should be behind elsif(MSCV) instead?
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _USE_MATH_DEFINES
        IMGUI_USER_CONFIG="imgui_config.h")
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    ${ADQ_CFLAGS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    Threads::Threads
    glfw
    ${ADQ_LIBRARIES}
)
