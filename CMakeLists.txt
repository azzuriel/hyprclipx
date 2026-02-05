cmake_minimum_required(VERSION 3.19)
project(hyprclipx
    VERSION 0.1.0
    DESCRIPTION "Layer-shell clipboard manager for Hyprland"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED hyprland)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(GTK4_LAYER REQUIRED gtk4-layer-shell-0)
pkg_check_modules(PANGO REQUIRED pango pangocairo)
pkg_check_modules(CAIRO REQUIRED cairo)

# ============================================================================
# Target 1: hyprclipx.so (Hyprland plugin - NO GTK, NO threads)
# ============================================================================
set(PLUGIN_SOURCES
    src/main.cpp
    src/Globals.cpp
    src/IPCHandler.cpp
    src/ConfigParser.cpp
)

add_library(hyprclipx SHARED ${PLUGIN_SOURCES})

target_include_directories(hyprclipx PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${HYPRLAND_INCLUDE_DIRS}
)

# Plugin links NOTHING except Hyprland (no GTK, no pango, no cairo)
target_compile_options(hyprclipx PRIVATE
    -Wall -Wextra -Wpedantic -fPIC
    ${HYPRLAND_CFLAGS_OTHER}
)

set_target_properties(hyprclipx PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ============================================================================
# Target 2: hyprclipx-ui (standalone GTK4 binary - NO Hyprland)
# ============================================================================
set(UI_SOURCES
    src/main_ui.cpp
    src/ClipboardRenderer.cpp
    src/ClipboardManager.cpp
    src/ConfigParser.cpp
)

add_executable(hyprclipx-ui ${UI_SOURCES})

target_include_directories(hyprclipx-ui PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTK4_INCLUDE_DIRS}
    ${GTK4_LAYER_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

target_link_libraries(hyprclipx-ui PRIVATE
    ${GTK4_LIBRARIES}
    ${GTK4_LAYER_LIBRARIES}
    ${PANGO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

target_compile_options(hyprclipx-ui PRIVATE
    -Wall -Wextra -Wpedantic
)

set_target_properties(hyprclipx-ui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Installation
install(TARGETS hyprclipx LIBRARY DESTINATION lib/hyprland/plugins)
install(TARGETS hyprclipx-ui RUNTIME DESTINATION bin)
