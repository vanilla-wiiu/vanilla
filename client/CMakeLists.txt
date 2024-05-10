find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Concurrent)
find_package(SDL2 REQUIRED)

add_executable(vanilla-gui
    syncdialog.cpp
    syncprogressdialog.cpp
    vanilla.cpp
    main.cpp
)

target_link_libraries(vanilla-gui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Concurrent
    SDL2
    vanilla
)

target_include_directories(vanilla-gui PRIVATE
    "${CMAKE_SOURCE_DIR}/server"
)