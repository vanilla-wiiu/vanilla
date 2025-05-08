add_library(vanilla STATIC
    gamepad/audio.c
    gamepad/command.c
    gamepad/gamepad.c
    gamepad/input.c
    gamepad/video.c
    util.c
    vanilla.c
)

target_include_directories(vanilla PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(vanilla PROPERTIES PUBLIC_HEADER "vanilla.h")

if (NOT ANDROID)
    target_link_libraries(vanilla PRIVATE
        pthread
    )
endif()

install(TARGETS vanilla)

if (VANILLA_BUILD_TESTS)
    function(add_test TEST_NAME TEST_FILES)
        add_executable(${TEST_NAME}
            ${TEST_FILES}
        )

        target_link_libraries(${TEST_NAME} PRIVATE vanilla m)
        target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    endfunction()
    
    add_test(bittest "test/bittest.c")
    add_test(reversebittest "test/reversebit.c")
    add_test(reversebitstresstest "test/reversebitstresstest.c")
endif()