cmake_minimum_required(VERSION 3.15)
project(xemu_wait_trace_plugin C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Configuration ---
set(XEMU_SOURCE_DIR "" CACHE PATH "Path to xemu source tree")

set(QEMU_PLUGIN_INCLUDE_DIR ${XEMU_SOURCE_DIR}/include/qemu)
if (NOT EXISTS "${QEMU_PLUGIN_INCLUDE_DIR}/qemu-plugin.h")
    message(FATAL_ERROR "qemu-plugin.h not found in '${QEMU_PLUGIN_INCLUDE_DIR}'. \
    Please set XEMU_SOURCE_DIR to the root of the xemu source code.")
endif ()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# --- Tracer Library (C++ - No QEMU headers) ---
add_library(
        tracer_lib
        STATIC
        src/tracer/Tracer.cpp
        src/tracer/Tracer.h
        src/tracer/TracerInterface.h
)

target_include_directories(
        tracer_lib
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Monitor Plugin (C - Includes QEMU headers) ---
add_library(
        monitor_plugin
        SHARED
        src/plugin.c
)

target_include_directories(
        monitor_plugin
        PRIVATE
        ${QEMU_PLUGIN_INCLUDE_DIR}
        ${GLIB_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
        monitor_plugin
        PRIVATE
        tracer_lib
)

if (APPLE)
    # macOS requires explicit permission to leave symbols undefined
    target_link_options(
        monitor_plugin
        PRIVATE
        "-undefined"
        "dynamic_lookup"
    )
elseif (UNIX)
    target_link_options(
        monitor_plugin
        PRIVATE
        "-Wl,-z,nodelete"
    )
else ()
    message(FATAL_ERROR "Unsupported platform")
endif ()

target_compile_options(
        monitor_plugin
        PRIVATE
        -Wall
        -Wextra
)
