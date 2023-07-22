# ShitStation
add_executable(ShitStation
        main.cpp
        core/emulator.cpp
        #        gui/gui.cpp
        core/events.hpp
        core/scheduler.cpp
        core/cpu.cpp
        core/bus.cpp
        core/gte.cpp
        core/cdrom.cpp
)

target_include_directories(ShitStation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ShitStation PRIVATE SDL2::SDL2-static glad fmt::fmt magic_enum::magic_enum BitField)


set_target_warnings(ShitStation WARNINGS_AS_ERRORS)


enable_sanitizers(ShitStation
        ${ENABLE_SANITIZER_ADDRESS}
        ${ENABLE_SANITIZER_LEAK}
        ${ENABLE_SANITIZER_THREAD}
        ${ENABLE_SANITIZER_MEMORY}
        ${ENABLE_SANITIZER_UNDEFINED_BEHAVIOR}
)
