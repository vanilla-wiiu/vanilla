add_executable(vanilla-pipe
    main.c
)

target_link_libraries(vanilla-pipe PRIVATE
    vanilla
    pthread
)

target_include_directories(vanilla-pipe PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
)

install(TARGETS vanilla-pipe)
