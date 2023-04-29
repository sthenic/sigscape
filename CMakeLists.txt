cmake_minimum_required(VERSION 3.15)
project(sigscape
  VERSION 0.1.0
  HOMEPAGE_URL "https://github.com/sthenic/sigscape"
  DESCRIPTION "GUI for ADQ3 series digitizers"
  LANGUAGES CXX C
)

add_executable(${PROJECT_NAME})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MOCK_ADQAPI "With this set to ON, only the simulator mode is available." OFF)

# Optional path to libadq. Set with -DLIBADQ_PATH=<path-to-libadq/ADQAPI>
# TODO: Allow setting the path to libadq and ADQAPI.h separately?
set(LIBADQ_PATH "" CACHE PATH "Optional path to libadq.so/ADQAPI.dll.")

if (APPLE)
  set(MOCK_ADQAPI ON)
endif()

# Get version control information from `git describe`
execute_process(COMMAND git describe --dirty
                OUTPUT_VARIABLE SIGSCAPE_REVISION
                OUTPUT_STRIP_TRAILING_WHITESPACE)
message("Revision is ${SIGSCAPE_REVISION}")

find_package(OpenGL REQUIRED)

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
    implot
    GIT_REPOSITORY https://github.com/epezent/implot
    GIT_TAG v0.14
)
FetchContent_MakeAvailable(implot)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.5
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
set(SKIP_INSTALL_ALL ON)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(
    png
    GIT_REPOSITORY https://github.com/glennrp/libpng
    GIT_TAG v1.6.39
)

set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
set(ZLIB_LIBRARIES zlibstatic)

set(PNG_BUILD_ZLIB ON CACHE BOOL "")
set(PNG_STATIC ON CACHE BOOL "")
set(PNG_SHARED OFF CACHE BOOL "")
set(PNG_EXECUTABLES OFF CACHE BOOL "")
set(PNG_TESTS OFF CACHE BOOL "")

FetchContent_MakeAvailable(png)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

find_package(Python3 REQUIRED Development)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
target_sources(${PROJECT_NAME} PRIVATE
    vendor/gl3w/gl3w.c
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
    src/persistent_directories.cpp
    src/data_processing.cpp
    src/digitizer.cpp
    src/identification.cpp
    src/file_watcher.cpp
    src/directory_watcher.cpp
    src/ui.cpp
    src/marker.cpp
    src/format.cpp
    src/data_types.cpp
    src/window.cpp
    src/embedded_python.cpp
    src/imgui_extensions.cpp
    src/main.cpp
)

if (UNIX)
    target_sources(${PROJECT_NAME} PRIVATE
        src/hotplug_linux.cpp
    )
elseif (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        src/hotplug_windows.cpp
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${implot_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${png_SOURCE_DIR}
    ${fmt_SOURCE_DIR}/include
    ${json_SOURCE_DIR}/include
    vendor/simple_fft
    vendor/gl3w
    include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GL3W
    SIGSCAPE_REVISION=\"${SIGSCAPE_REVISION}\"
)

if (MOCK_ADQAPI)
    message(STATUS "Using MOCK_ADQAPI (no ADQAPI dependency).")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MOCK_ADQAPI)
    target_sources(${PROJECT_NAME} PRIVATE
        src/mock/adqapi.cpp
        src/mock/digitizer.cpp
        src/mock/system_manager.cpp
        src/mock/generator.cpp)
endif()

if (UNIX)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        IMGUI_USER_CONFIG="imgui_config.h")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wswitch -Werror=overflow)
elseif (WIN32)
    # TODO: Maybe this should be behind elsif(MSCV) instead?
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _USE_MATH_DEFINES
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        IMGUI_USER_CONFIG="imgui_config.h")
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4) # FIXME: Once we had /WX too
endif()

# TODO: Split into FindAdq.cmake to use with find_package?
if (NOT MOCK_ADQAPI AND NOT TARGET adq)
    # Create imported library it the adq target doesn't exist (i.e. included from other sources)
    add_library(adq SHARED IMPORTED)

    if (WIN32)
        if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
            list(APPEND LIBADQ_PATH "$ENV{PROGRAMFILES}/SP Devices/ADQAPI_x64")
        else()
            list(APPEND LIBADQ_PATH "$ENV{PROGRAMFILES}/SP Devices/ADQAPI")
        endif()

        find_library(ADQ_LIBRARIES NAMES ADQAPI HINTS ${LIBADQ_PATH} REQUIRED)
        find_path(ADQ_INCLUDE_PATH NAMES ADQAPI.h HINTS ${LIBADQ_PATH} REQUIRED)
        find_file(ADQ_DLL NAMES ADQAPI.dll HINTS ${LIBADQ_PATH} REQUIRED)

        set_target_properties(adq PROPERTIES
            IMPORTED_LOCATION ${ADQ_DLL}
            INTERFACE_INCLUDE_DIRECTORIES ${ADQ_INCLUDE_PATH}
            IMPORTED_IMPLIB ${ADQ_LIBRARIES})
    else()
        find_library(ADQ_LIBRARIES NAMES adq HINTS ${LIBADQ_PATH} REQUIRED)
        find_path(ADQ_INCLUDE_PATH NAMES ADQAPI.h HINTS ${LIBADQ_PATH} REQUIRED)
        set_target_properties(adq PROPERTIES
            IMPORTED_LOCATION ${ADQ_LIBRARIES}
            INTERFACE_INCLUDE_DIRECTORIES ${ADQ_INCLUDE_PATH}
        )
    endif()
endif()

if (WIN32)
    # Copy ADQAPI.dll to the target (.exe) directory.
    if (NOT MOCK_ADQAPI)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:adq>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endif()

    # Set the subsystem to Windows GUI application in release configuration (no
    # console window). The linker option sets the entrypoint to 'main' (not
    # 'WinMain') which is the default.
    # The debug configuration keeps the console window for stdout and stderr
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()


target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    Threads::Threads
    glfw
    fmt
    png_static
    Python3::Python
)

# Link against libadq unless MOCK_ADQAPI is set.
if (NOT MOCK_ADQAPI)
    target_link_libraries(${PROJECT_NAME} PRIVATE adq)
endif()

# Platform-specific libraries (for hotplug detection).
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE cfgmgr32)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE udev)
endif()

# Add the test suite as subdirectory (excluded by default).
add_subdirectory(test EXCLUDE_FROM_ALL)

install(TARGETS ${PROJECT_NAME})

# Include GNUInstallDirs to get CMAKE_INSTALL_DOCDIR
include(GNUInstallDirs)

# Install documents in CMAKE_INSTALL_DOCDIR ("share/doc/<PROJECT_NAME>")
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES THIRD_PARTY_LICENSES.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)

# Set the install directory to "sigscape". The default is "sigscape <VERSION>"
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${SIGSCAPE_REVISION}-${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_PACKAGE_CONTACT "Marcus Eriksson")

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)

# Required for NSIS start menu shortcut
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME};${PROJECT_NAME}")

# The branding text at the bottom of the installer. Default is "Nullsoft Install System vX.YY")
set(CPACK_NSIS_BRANDING_TEXT " ")

# TODO: Figure of if needed?
set(CPACK_NSIS_URL_INFO_ABOUT ${CMAKE_PROJECT_HOMEPAGE_URL})

include(CPack)
