find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Concurrent)
find_package(SDL2 REQUIRED)

add_executable(vanilla
    syncdialog.cpp
    syncprogressdialog.cpp
    vanilla.cpp
    main.cpp
)

target_link_libraries(vanilla PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Concurrent
    SDL2
    vanillaserver
)

target_include_directories(vanilla PRIVATE
    "${CMAKE_SOURCE_DIR}/server"
)