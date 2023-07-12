cmake_minimum_required(VERSION 3.26)
project(
        shitstation
        VERSION 0.1.0
        LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(ENABLE_SANITIZER_ADDRESS OFF CACHE BOOL "Enable Address Sanitizer if available" FORCE)
set(ENABLE_SANITIZER_LEAK OFF CACHE BOOL "Enable Leak Sanitizer if available" FORCE)
set(ENABLE_SANITIZER_THREAD OFF CACHE BOOL "Enable Thread Sanitizer if available" FORCE)
set(ENABLE_SANITIZER_MEMORY OFF CACHE BOOL "Enable Memory Sanitizer if available" FORCE)
set(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR OFF CACHE BOOL "Enable Undefined Behavior Sanitizer if available" FORCE)
set(ENABLE_CACHE ON CACHE BOOL "Strictly disable caching" FORCE)
set(ENABLE_IPO OFF CACHE BOOL "Enable Interprocedural Optimization" FORCE)
set(WARNINGS_AS_ERRORS ON CACHE BOOL "Enable Warnings as Errors" FORCE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(cmake/PreventInSourceBuilds.cmake)
include(cmake/Cache.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/StandardProjectSettings.cmake)
include(cmake/IPO.cmake)

set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory(deps/glad)
add_subdirectory(deps/fmt)
add_subdirectory(deps/magic_enum)

if (!WIN32)
    find_package(SDL2 REQUIRED)
endif()

if (!SDL2_FOUND)
    message("SDL2 not found, building from source...")
    add_subdirectory(deps/SDL)
endif()

#IMGUI
set(IMGUI_DIR deps/imgui)
set(IMGUI_INCLUDE_DIRS deps/imgui deps/imgui/backends)

add_library(ImGui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ImGui PUBLIC ${IMGUI_INCLUDE_DIRS})
target_link_libraries(ImGui PRIVATE SDL2::SDL2-static)

if (ENABLE_CACHE)
    try_enable_cache()
endif()

if (ENABLE_IPO)
    enable_ipo()
endif()

add_subdirectory(src)
