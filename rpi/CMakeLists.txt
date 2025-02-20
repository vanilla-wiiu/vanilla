cmake_minimum_required(VERSION 3.22)

set(_sdl_lib SDL2::SDL2)
set(_vanilla_pi_libs "")

if (VANILLA_BUILD_VENDORED)
    # Static build
    if (NOT ANDROID)
        set(BUILD_SHARED_LIBS OFF)

        set(_sdl_lib SDL2::SDL2-static)
    endif()

    # Link zlib for PNG
    if (UNIX)
        list(APPEND _vanilla_pi_libs "z")
    endif()

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
    set(LIBXML2_WITH_PROGRAMS OFF)
    FetchContent_Declare(
        libxml2
        GIT_REPOSITORY https://gitlab.gnome.org/GNOME/libxml2.git
        GIT_TAG v2.13.5
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(SDL2 SDL2_ttf SDL2_image libxml2)
    set(FFMPEG_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg/install")
    set(FFMPEG_CONFIGURE_ARGS
        --enable-gpl
        --disable-shared
        --enable-static
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
    if (ANDROID)
        list(APPEND FFMPEG_CONFIGURE_ARGS
            --enable-mediacodec
            --enable-jni
            --enable-hwaccel=h264_mediacodec
            --enable-decoder=h264_mediacodec
        )
        list(APPEND _vanilla_pi_libs
            android
            mediandk
        )
    endif()
    if (CMAKE_CROSSCOMPILING)
        set(FFMPEG_TARGET_OS ${CMAKE_SYSTEM_NAME})
        set(FFMPEG_ENV "")
        if (MINGW)
            set(FFMPEG_TARGET_OS "mingw32")
            list(APPEND FFMPEG_CONFIGURE_ARGS
                --cross-prefix=${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32-
                --host-cc=${CMAKE_C_COMPILER} # HACK: FFmpeg complains about "lack of C11 support" without this because it can't find "gcc"
            )
        elseif(MSVC)
            set(FFMPEG_TARGET_OS "win32")
        elseif(APPLE)
            set(FFMPEG_TARGET_OS "darwin")
        elseif(ANDROID)
            set(FFMPEG_TARGET_OS "android")
            string(REPLACE "-" "" _android_ver ${ANDROID_PLATFORM})
            # message(FATAL_ERROR ${_android_ver})
            list(APPEND FFMPEG_CONFIGURE_ARGS
                --cross-prefix=${CMAKE_SYSTEM_PROCESSOR}-linux-${_android_ver}-
                # --cc=${CMAKE_C_COMPILER}
                # --host-cc=${CMAKE_C_COMPILER}
                # --ld=${CMAKE_C_COMPILER}
                # --host-ld=${CMAKE_C_COMPILER}
                --host-cc=${CMAKE_SYSTEM_PROCESSOR}-linux-${_android_ver}-clang
                --extra-cflags=${CMAKE_C_FLAGS}
                --extra-ldflags=${CMAKE_STATIC_LINKER_FLAGS}
                --sysroot=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/sysroot
                --ar=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-ar
                --ranlib=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-ranlib
                --disable-asm
                --nm=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-nm
                --strip=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-strip
            )
            list(APPEND FFMPEG_ENV "PATH=$ENV{PATH}:${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin")
        elseif(LINUX)
            set(FFMPEG_TARGET_OS "linux")
            list(APPEND FFMPEG_CONFIGURE_ARGS
                --cross-prefix=${CMAKE_CROSS_PREFIX}
                --host-cc=${CMAKE_C_COMPILER}
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
        add_library("FFmpeg::${_component}" SHARED IMPORTED)
        if (MSVC)
            set(_component_lib "${FFMPEG_INSTALL_DIR}/bin/${_component}.lib")
        else()
            set(_component_lib "${FFMPEG_INSTALL_DIR}/lib/lib${_component}.a")
        endif()
        set_target_properties("FFmpeg::${_component}" PROPERTIES
            IMPORTED_IMPLIB ${_component_lib}
            IMPORTED_LOCATION ${_component_lib}
        )
        list(APPEND FFMPEG_FILES ${_component_lib})
        add_dependencies("FFmpeg::${_component}" FFmpeg)
    endforeach()

    list(APPEND FFMPEG_FILES ${FFMPEG_INCLUDE_DIR})
    include_directories(${FFMPEG_INCLUDE_DIR})

    ExternalProject_Add(FFmpeg
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
        GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
        GIT_TAG n7.1
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${FFMPEG_ENV} ../FFmpeg/configure ${FFMPEG_CONFIGURE_ARGS}
        BUILD_COMMAND ${CMAKE_COMMAND} -E env ${FFMPEG_ENV} make -j${CMAKE_BUILD_PARALLEL_LEVEL}
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
    menu/menu_region.c
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

# Ensure executable is installed
install(TARGETS vanilla-pi)

# Find packages (unless using vendored build)
if (NOT VANILLA_BUILD_VENDORED)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(SDL2_image REQUIRED)
endif()

target_link_libraries(vanilla-pi PRIVATE
    ${_sdl_lib}
    SDL2::SDL2main
    SDL2_ttf
    SDL2_image
    vanilla
    m
)

if (ANDROID)
    target_link_libraries(vanilla-pi PRIVATE
        SDL2main
        log
    )
endif()

# Find FFmpeg
if (NOT VANILLA_BUILD_VENDORED)
    find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swscale)
endif()

target_link_libraries(vanilla-pi PRIVATE
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    ${_vanilla_pi_libs}
)

if (LINUX)
    # Assume this platform is capable of running vanilla-pipe
    target_compile_definitions(vanilla-pi PRIVATE VANILLA_PIPE_AVAILABLE)

    # Link with DRM if available
    find_package(DRM)
    if (DRM_FOUND)
        target_link_libraries(vanilla-pi PRIVATE ${DRM_LIBRARIES})
        target_include_directories(vanilla-pi PRIVATE ${DRM_INCLUDE_DIRS})
    endif()
endif()

# Find libxml2 (used for storing/parsing user config)
if (VANILLA_BUILD_VENDORED)
    target_link_libraries(vanilla-pi PRIVATE LibXml2::LibXml2)
else()
    find_package(LibXML2 REQUIRED)
    target_link_libraries(vanilla-pi PRIVATE ${LibXML2_LIBRARIES})
    target_include_directories(vanilla-pi PRIVATE ${LibXML2_INCLUDE_DIR})
endif()


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
)

# Install assets
if(ANDROID)
    # Put assets in correct place relative to build
    add_custom_command(
        TARGET vanilla-pi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_CURRENT_SOURCE_DIR}/../android/app/src/main/assets"
    )
else()
    # Put assets in correct place relative to build
    add_custom_command(
        TARGET vanilla-pi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../share/vanilla/assets"
    )

    if(WIN32)
        set(_vanilla_pi_asset_destination "bin")
    else()
        set(_vanilla_pi_asset_destination "share/vanilla")
    endif()
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${_vanilla_pi_asset_destination}")

    if (LINUX)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/com.mattkc.vanilla.desktop" DESTINATION share/applications)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/com.mattkc.vanilla.svg" DESTINATION share/pixmaps)
    endif()
endif()
