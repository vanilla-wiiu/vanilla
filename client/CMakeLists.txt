find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Concurrent OpenGLWidgets)
find_package(SDL2 REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil swscale)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(vanilla-gui
    backend.cpp
    gamepadhandler.cpp
    syncdialog.cpp
    syncprogressdialog.cpp
    main.cpp
    mainwindow.cpp
    videodecoder.cpp
    viewer.cpp
)

target_link_libraries(vanilla-gui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::OpenGLWidgets
    SDL2
    vanilla
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
)

target_include_directories(vanilla-gui PRIVATE
    "${CMAKE_SOURCE_DIR}/server"
)