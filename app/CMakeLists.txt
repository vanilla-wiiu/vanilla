find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia OpenGLWidgets Network)
find_package(SDL2 REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil avfilter)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(vanilla-gui
    audiohandler.cpp
    backend.cpp
    gamepadhandler.cpp
    keymap.cpp
    inputconfigdialog.cpp
    syncdialog.cpp
    syncprogressdialog.cpp
    main.cpp
    mainwindow.cpp
    resource.qrc
    videodecoder.cpp
    viewer.cpp
)

target_link_libraries(vanilla-gui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::OpenGLWidgets
    SDL2
    vanilla
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::avfilter
)

target_include_directories(vanilla-gui PRIVATE
    "${CMAKE_SOURCE_DIR}/lib"
)

install(TARGETS vanilla-gui)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/com.mattkc.vanilla.desktop" DESTINATION share/applications)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/com.mattkc.vanilla.svg" DESTINATION share/pixmaps)