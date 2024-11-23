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

if (VANILLA_BUILD_TESTS)
    include(CTest)
    function(vanilla_add_test TEST_NAME TEST_FILES)
        add_executable(${TEST_NAME}
            ${TEST_FILES}
        )

        target_link_libraries(${TEST_NAME} PRIVATE vanilla m)
        target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    vanilla_add_test(bittest "test/bittest.c")
    vanilla_add_test(reversebittest "test/reversebit.c")
    vanilla_add_test(reversebitstresstest "test/reversebitstresstest.c")
endif()
