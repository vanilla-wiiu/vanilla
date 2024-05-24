add_executable(vanilla-pipe
    common.c
    main.c
)

target_link_libraries(vanilla-pipe PRIVATE
    vanilla
)

target_include_directories(vanilla-pipe PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
)