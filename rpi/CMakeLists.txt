find_package(SDL2 REQUIRED)

add_executable(vanilla-pi
    main.c
)

target_include_directories(vanilla-pi PRIVATE
    "${CMAKE_SOURCE_DIR}/lib"
)

target_link_libraries(vanilla-pi PRIVATE
    SDL2::SDL2
    vanilla
)