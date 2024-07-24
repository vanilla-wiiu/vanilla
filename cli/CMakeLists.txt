add_executable(vanilla-cli
    main.c
)

target_link_libraries(vanilla-cli PRIVATE
    vanilla
)

target_include_directories(vanilla-cli PRIVATE
    "${CMAKE_SOURCE_DIR}/lib"
)

install(TARGETS vanilla-cli)