find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil avfilter swscale)
find_package(libxml2 REQUIRED)

add_executable(vanilla-pi
    config.c
    drm.c
    lang.c
    game/game_decode.c
    game/game_main.c
    main2.c
    menu/menu.c
    menu/menu_common.c
    menu/menu_delete.c
    menu/menu_edit.c
    menu/menu_game.c
    menu/menu_gamepad.c
    menu/menu_main.c
    menu/menu_settings.c
    menu/menu_sudo.c
    menu/menu_sync.c
    ui/ui.c
    ui/ui_anim.c
    ui/ui_sdl.c
    ui/ui_util.c
    ui/ui_util.h
)

install(TARGETS vanilla-pi)

target_include_directories(vanilla-pi PRIVATE
    "${CMAKE_SOURCE_DIR}/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"

    # xf86drm.h needs this
    /usr/include/libdrm
)

target_link_libraries(vanilla-pi PRIVATE
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    SDL2_image::SDL2_image
    vanilla
    FFmpeg::avfilter
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    LibXml2::LibXml2

    drm
    m
)
