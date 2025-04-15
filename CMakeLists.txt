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
option(EMBEDDED_PYTHON "Set to OFF to disable support for embedded python." ON)

# Optional path to libadq. Set with -DLIBADQ_PATH=<path-to-libadq/ADQAPI>
set(LIBADQ_PATH "" CACHE PATH "Optional path to libadq.so/ADQAPI.dll.")
set(LIBADQ_IPATH "" CACHE PATH "Optional path to ADQAPI.h (if other than LIBADQ_PATH).")

if (APPLE)
  set(MOCK_ADQAPI ON)
endif()

# Get version control information from `git describe`
execute_process(COMMAND git -C ${CMAKE_CURRENT_LIST_DIR} describe --dirty
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
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
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
    GIT_REPOSITORY https://github.com/sthenic/implot
    GIT_TAG 0b0ee91a749e1ef63e6a99470a31fde9f9d90ee6
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(implot)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG 9.1.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG v1.2.13
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
set(SKIP_INSTALL_ALL ON)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(
    png
    GIT_REPOSITORY https://github.com/glennrp/libpng
    GIT_TAG v1.6.39
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
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
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG v1.11.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(spdlog)

# On Windows, we use (or resort) to run-time dynamic linking. Our goal is to
# integrate with the 'native' Python environment (and not ship our own).
# However, the crux is that a 'native' Python environment on Windows isn't that
# well defined.
if (NOT WIN32)
    find_package(Python3 REQUIRED Development)
endif()

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
    src/embedded_python_thread.cpp
    src/imgui_extensions.cpp
    src/screenshot.cpp
    src/log.cpp
    src/record_frame.cpp
    src/main.cpp
)

if (UNIX)
    target_sources(${PROJECT_NAME} PRIVATE
        src/hotplug_linux.cpp
    )
elseif (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        src/hotplug_windows.cpp
        data/sigscape.rc
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${implot_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${png_SOURCE_DIR}
    ${fmt_SOURCE_DIR}/include
    ${json_SOURCE_DIR}/include
    ${spdlog_SOURCE_DIR}/include
    vendor/simple_fft
    vendor/gl3w
    vendor/stb
    data
    include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GL3W
    SIGSCAPE_REVISION=\"${SIGSCAPE_REVISION}\"
    IMGUI_USER_CONFIG="imgui_config.h"
    $<$<BOOL:${EMBEDDED_PYTHON}>:EMBEDDED_PYTHON>
)

message(STATUS "Embedded Python is ${EMBEDDED_PYTHON}.")

if (MOCK_ADQAPI)
    # Build a mocked shared library, whose CMakeLists.txt is located in
    # src/mock.
    message(STATUS "Using MOCK_ADQAPI (no ADQAPI dependency).")
    add_subdirectory(mock _adq)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        MOCK_ADQAPI
        MOCK_ADQAPI_PATH=\"$<TARGET_FILE:adq>\")
endif()

if (UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wswitch -Wunused -Wshadow=local -Werror=overflow)
elseif (WIN32)
    # TODO: Maybe this should be behind elsif(MSCV) instead?
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
        NOMINMAX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4) # FIXME: Once we had /WX too
endif()

# TODO: Split into FindAdq.cmake to use with find_package?
if (NOT MOCK_ADQAPI AND NOT TARGET adq)
    # Create imported library it the adq target doesn't exist (i.e. included from other sources)
    add_library(adq SHARED IMPORTED)

    if (LIBADQ_PATH)
        message(STATUS "Using ADQ library hint '${LIBADQ_PATH}'.")
    endif()
    if (LIBADQ_IPATH)
        message(STATUS "Using ADQ include hint '${LIBADQ_IPATH}'.")
    endif()

    if (WIN32)
        if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
            list(APPEND LIBADQ_PATH "$ENV{PROGRAMFILES}/SP Devices/ADQAPI_x64")
        else()
            list(APPEND LIBADQ_PATH "$ENV{PROGRAMFILES}/SP Devices/ADQAPI")
        endif()

        find_library(ADQ_LIBRARIES NAMES ADQAPI HINTS ${LIBADQ_PATH} REQUIRED)
        find_path(ADQ_INCLUDE_PATH NAMES ADQAPI.h HINTS ${LIBADQ_IPATH} ${LIBADQ_PATH} REQUIRED)
        find_file(ADQ_DLL NAMES ADQAPI.dll HINTS ${LIBADQ_PATH} REQUIRED)

        set_target_properties(adq PROPERTIES
            IMPORTED_LOCATION ${ADQ_DLL}
            INTERFACE_INCLUDE_DIRECTORIES ${ADQ_INCLUDE_PATH}
            IMPORTED_IMPLIB ${ADQ_LIBRARIES})

        message(STATUS "Using ADQ DLL '${ADQ_DLL}'.")
    else()
        find_library(ADQ_LIBRARIES NAMES adq HINTS ${LIBADQ_PATH} REQUIRED)
        find_path(ADQ_INCLUDE_PATH NAMES ADQAPI.h HINTS ${LIBADQ_IPATH} ${LIBADQ_PATH} REQUIRED)
        set_target_properties(adq PROPERTIES
            IMPORTED_LOCATION ${ADQ_LIBRARIES}
            INTERFACE_INCLUDE_DIRECTORIES ${ADQ_INCLUDE_PATH}
        )
    endif()

    message(STATUS "Using ADQ library '${ADQ_LIBRARIES}'.")
    message(STATUS "Using ADQ include path '${ADQ_INCLUDE_PATH}'.")
endif()

if (WIN32)
    # Copy ADQAPI.dll to the target (.exe) directory.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:adq>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)

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
    adq
)

# Platform-specific libraries (for hotplug detection).
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        cfgmgr32)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        udev
        Python3::Python)
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

if(UNIX)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/sigscape.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/sigscape128.png DESTINATION share/pixmaps)
endif()

set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)

# Set the install directory to "sigscape". The default is "sigscape <VERSION>"
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${SIGSCAPE_REVISION}-${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_PACKAGE_CONTACT "Marcus Eriksson")

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Required for NSIS start menu shortcut
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME};${PROJECT_NAME}")

# The branding text at the bottom of the installer. Default is "Nullsoft Install System vX.YY")
set(CPACK_NSIS_BRANDING_TEXT " ")
set(CPACK_NSIS_MUI_ICON ${CMAKE_CURRENT_SOURCE_DIR}/data/sigscape.ico)
set(CPACK_NSIS_MUI_UNIICON ${CMAKE_CURRENT_SOURCE_DIR}/data/sigscape.ico)

# TODO: Figure of if needed?
set(CPACK_NSIS_URL_INFO_ABOUT ${CMAKE_PROJECT_HOMEPAGE_URL})

# WIX
if(WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt COPYONLY)
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt)
endif()
set(CPACK_WIX_UPGRADE_GUID "7e1de5a6-9759-456c-a962-469e0d35f5a9")
set(CPACK_WIX_PRODUCT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/data/sigscape.ico)

include(CPack)
