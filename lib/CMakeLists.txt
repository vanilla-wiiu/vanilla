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

if (0)
    add_executable(bittest
        bittest.c
    )

    target_link_libraries(bittest PRIVATE vanilla m)
    target_include_directories(bittest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()