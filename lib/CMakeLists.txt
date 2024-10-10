add_library(vanilla STATIC
    gamepad/audio.c
    gamepad/command.c
    gamepad/gamepad.c
    gamepad/input.c
    gamepad/video.c
    status.c
    util.c
    vanilla.c
)

target_include_directories(vanilla PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (NOT ANDROID)
    target_link_libraries(vanilla PRIVATE
        pthread
    )
endif()

install(TARGETS vanilla)
