cmake_minimum_required(VERSION 3.20)
project(
        ShitStation
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
set(WARNINGS_AS_ERRORS OFF CACHE BOOL "Enable Warnings as Errors" FORCE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(cmake/PreventInSourceBuilds.cmake)
include(cmake/Cache.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/StandardProjectSettings.cmake)
include(cmake/IPO.cmake)

if(WIN32)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(SDL_MAIN_HANDLED)
endif()

add_compile_definitions(GL_SILENCE_DEPRECATION)

if(APPLE)
    find_package(SDL2 REQUIRED)
else()
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(deps/SDL)
endif()

add_subdirectory(deps/glad)
add_subdirectory(deps/fmt)
add_subdirectory(deps/magic_enum)
add_subdirectory(deps/Dolphin)

if (ENABLE_CACHE)
    try_enable_cache()
endif()

if (ENABLE_IPO)
    enable_ipo()
endif()

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/support/log.hpp
        src/support/helpers.hpp
        src/cpu/cpu.cpp
        src/cpu/cpu.hpp
        src/support/register.hpp
        src/bus/bus.cpp
        src/bus/bus.hpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static glad fmt::fmt magic_enum::magic_enum BitField)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_warnings(${PROJECT_NAME} WARNINGS_AS_ERRORS)


enable_sanitizers(ShitStation
        ${ENABLE_SANITIZER_ADDRESS}
        ${ENABLE_SANITIZER_LEAK}
        ${ENABLE_SANITIZER_THREAD}
        ${ENABLE_SANITIZER_MEMORY}
        ${ENABLE_SANITIZER_UNDEFINED_BEHAVIOR}
)
