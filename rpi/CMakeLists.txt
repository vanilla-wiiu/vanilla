find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil avfilter swscale)

add_executable(vanilla-pi
    drm.c
    main2.c
    menu/menu.c
    ui/ui.c
    ui/ui_sdl2.c
)

install(TARGETS vanilla-pi)

target_include_directories(vanilla-pi PRIVATE
    "${CMAKE_SOURCE_DIR}/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}"

    # xf86drm.h needs this
    /usr/include/libdrm
)

target_link_libraries(vanilla-pi PRIVATE
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    vanilla
    FFmpeg::avfilter
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale

    drm
    m
)
