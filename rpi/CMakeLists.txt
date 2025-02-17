if (VANILLA_BUILD_VENDORED)
    include(FetchContent)
    include(ExternalProject)

    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.32.0
    )

    set(SDL2TTF_VENDORED ON)
    FetchContent_Declare(
        SDL2_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-2.24.0
    )

    FetchContent_Declare(
        SDL2_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-2.8.5
    )

    set(LIBXML2_WITH_PYTHON OFF)
    set(LIBXML2_WITH_ICONV OFF)
    FetchContent_Declare(
        libxml2
        GIT_REPOSITORY https://gitlab.gnome.org/GNOME/libxml2.git
        GIT_TAG v2.13.5
    )

    FetchContent_MakeAvailable(SDL2 SDL2_ttf SDL2_image libxml2)
    set(FFMPEG_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg/install")
    set(FFMPEG_CONFIGURE_ARGS
        --enable-gpl
        --enable-shared
        --disable-programs
        --prefix=${FFMPEG_INSTALL_DIR}
        --disable-doc
        --disable-swresample
        --disable-postproc
        --disable-avfilter
        --disable-encoders
        --disable-decoders
        --disable-demuxers
        --disable-muxers
        --disable-network
        --enable-muxer=mp4
        --enable-muxer=image2
        --enable-decoder=h264
        --enable-encoder=png
    )
    if (CMAKE_CROSSCOMPILING)
        set(FFMPEG_TARGET_OS ${CMAKE_SYSTEM_NAME})
        if (MINGW)
            set(FFMPEG_TARGET_OS "mingw32")
            list(APPEND FFMPEG_CONFIGURE_ARGS
                --cross-prefix=${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32-
            )
        elseif(MSVC)
            set(FFMPEG_TARGET_OS "win32")
        elseif(APPLE)
            set(FFMPEG_TARGET_OS "darwin")
        elseif(ANDROID)
            set(FFMPEG_TARGET_OS "android")
            list(APPEND FFMPEG_CONFIGURE_ARGS
                --cross-prefix=${CMAKE_SYSTEM_PROCESSOR}-linux-android35-
                --cc=/opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/${CMAKE_SYSTEM_PROCESSOR}-linux-android35-clang
                --extra-cflags=${CMAKE_C_FLAGS}
                --ar=/opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar
                --ranlib=/opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ranlib
                --disable-asm
                --nm=/opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm
                --strip=/opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip
            )
        endif()
        list(APPEND FFMPEG_CONFIGURE_ARGS
            --enable-cross-compile
            --arch=${CMAKE_SYSTEM_PROCESSOR}
            --target-os=${FFMPEG_TARGET_OS}
        )
    endif()

    set(FFMPEG_FILES "")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_INSTALL_DIR}/include")
    foreach(_component avformat avcodec avutil swscale)
        add_library("FFmpeg::${_component}" UNKNOWN IMPORTED)
        if (WIN32)
            set(_component_lib "${FFMPEG_INSTALL_DIR}/bin/${_component}.lib")
        else()
            set(_component_lib "${FFMPEG_INSTALL_DIR}/lib/lib${_component}.so")
        endif()
        set_target_properties("FFmpeg::${_component}" PROPERTIES
            IMPORTED_LOCATION ${_component_lib})
        list(APPEND FFMPEG_FILES ${_component_lib})
        add_dependencies("FFmpeg::${_component}" FFmpeg)
    endforeach()

    list(APPEND FFMPEG_FILES ${FFMPEG_INCLUDE_DIR})
    include_directories(${FFMPEG_INCLUDE_DIR})

    ExternalProject_Add(FFmpeg
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
        GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
        GIT_TAG n7.1
        CONFIGURE_COMMAND ../FFmpeg/configure ${FFMPEG_CONFIGURE_ARGS}
        # BUILD_COMMAND "make -j${CMAKE_BUILD_PARALLEL_LEVEL}"
        UPDATE_COMMAND ""
        BUILD_BYPRODUCTS
            ${FFMPEG_FILES}
    )
endif()

list(APPEND VANILLA_PI_SRC
    config.c
    def.c
    # drm.c
    lang.c
    #game/game_decode_ffmpeg.c
    game/game_decode.c
    game/game_main.c
    main.c
    menu/menu.c
    menu/menu_common.c
    menu/menu_connection.c
    menu/menu_delete.c
    menu/menu_edit.c
    menu/menu_game.c
    menu/menu_gamepad.c
    menu/menu_main.c
    menu/menu_rename.c
    menu/menu_settings.c
    menu/menu_sync.c
    pipemgmt.c
    platform.c
    ui/ui.c
    ui/ui_anim.c
    ui/ui_sdl.c
    ui/ui_util.c
    ui/ui_util.h
)

# Declare executable
if (ANDROID)
    add_library(vanilla-pi SHARED ${VANILLA_PI_SRC})
else()
    add_executable(vanilla-pi ${VANILLA_PI_SRC})
endif()

if (UNIX AND NOT APPLE)
    # Assume this platform is capable of running vanilla-pipe
    target_compile_definitions(vanilla-pi PRIVATE VANILLA_PIPE_AVAILABLE)
endif()

# Ensure executable is installed
install(TARGETS vanilla-pi)

# Find packages (unless using vendored build)
if (NOT VANILLA_BUILD_VENDORED)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(SDL2_image REQUIRED)
endif()

target_link_libraries(vanilla-pi PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    SDL2_ttf::SDL2_ttf
    SDL2_image::SDL2_image
    vanilla

    # drm
    m
)

if (ANDROID)
    target_link_libraries(vanilla-pi PRIVATE
        SDL2main
        log
    )
endif()

# Find FFmpeg
#
# On Android, we use MediaCodec directly
if (NOT VANILLA_BUILD_VENDORED)
    find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swscale)
endif()

# if (NOT ANDROID)
    target_link_libraries(vanilla-pi PRIVATE
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::avutil
        FFmpeg::swscale
    )
# endif()

# Find libxml2 (used for storing/parsing user config)
#
# On Android, we compile as part of the build, so we don't need to find it
if (NOT VANILLA_BUILD_VENDORED)
    find_package(libxml2 REQUIRED)
endif()

target_link_libraries(vanilla-pi PRIVATE LibXml2::LibXml2)

# Link with winsock2 on Windows
if (WIN32)
    set_target_properties(vanilla-pi PROPERTIES WIN32_EXECUTABLE true)
    target_link_libraries(vanilla-pi PRIVATE wsock32 ws2_32)
endif()

# Set up include directories around the repository
target_include_directories(vanilla-pi PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."

    # xf86drm.h needs this
    /usr/include/libdrm
)
