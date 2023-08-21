set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
cmake_minimum_required(VERSION 3.20)
project(
        ShitStation
        VERSION 0.1.0
        LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# Check Git submodules.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.gitmodules")
    execute_process (COMMAND git submodule update --init
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

option(ENABLE_SANITIZER_ADDRESS  "Enable Address Sanitizer if available" OFF)
option(ENABLE_SANITIZER_LEAK  "Enable Leak Sanitizer if available" OFF)
option(ENABLE_SANITIZER_THREAD  "Enable Thread Sanitizer if available" OFF)
option(ENABLE_SANITIZER_MEMORY  "Enable Memory Sanitizer if available" OFF)
option(ENABLE_SANITIZER_UNDEFINED "Enable Undefined Behavior Sanitizer if available" OFF)
option(ENABLE_CACHE "Strictly disable caching" ON)
option(ENABLE_IPO  "Enable Interprocedural Optimization" ON)
option(WARNINGS_AS_ERRORS "Enable Warnings as Errors" OFF)
option(COPY_RESOURCES "Copy BIOS/resources to build folder" OFF)
option(BUILD_DOCS "Build documentation" OFF)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(cmake/Doxygen.cmake)
include(cmake/PreventInSourceBuilds.cmake)
include(cmake/Cache.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/StandardProjectSettings.cmake)
include(cmake/IPO.cmake)

if (BUILD_DOCS)
    enable_doxygen()
endif()

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

if (ENABLE_IPO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    message("Enabling IPO")
    enable_ipo()
endif()

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/support/log.hpp
        src/support/helpers.hpp
        src/cpu/cpu.cpp
        src/cpu/cpu.hpp
        #src/support/register.hpp
        src/bus/bus.cpp
        src/bus/bus.hpp
        src/psx.cpp
        src/psx.hpp
        src/scheduler/scheduler.cpp
        src/scheduler/scheduler.hpp
        src/dma/dmacontroller.cpp
        src/dma/dmacontroller.hpp
        src/timers/timers.cpp
        src/timers/timers.hpp
        src/gpu/gpu.cpp
        src/gpu/gpu.hpp
        src/cdrom/cdrom.cpp
        src/cdrom/cdrom.hpp
        src/cdrom/cdrom_util.hpp
        src/support/fifo.hpp
        src/sio/sio.cpp
        src/sio/sio.hpp
        src/gpu/gpugl.cpp
        src/gpu/gpugl.hpp
        src/support/opengl.hpp
        src/gpu/softgpu.cpp
        src/gpu/softgpu.hpp
        src/spu/spu.cpp
        src/spu/spu.hpp)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static glad fmt::fmt magic_enum::magic_enum BitField)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_warnings(${PROJECT_NAME} ${WARNINGS_AS_ERRORS})


enable_sanitizers(${PROJECT_NAME}
        ${ENABLE_SANITIZER_ADDRESS}
        ${ENABLE_SANITIZER_LEAK}
        ${ENABLE_SANITIZER_UNDEFINED}
        ${ENABLE_SANITIZER_THREAD}
        ${ENABLE_SANITIZER_MEMORY}
)

if (COPY_RESOURCES)
add_custom_command(TARGET ShitStation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/SCPH1001.BIN $<TARGET_FILE_DIR:ShitStation>/)
endif()
