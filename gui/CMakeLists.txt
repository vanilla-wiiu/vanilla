cmake_minimum_required(VERSION 3.22)

set(_sdl_lib SDL2::SDL2)
set(_vanilla_gui_libs "")
set(_ffmpeg_components avformat avcodec avutil swscale)

function(compile_ffmpeg _target_name _system_name _system_processor)
	set(FFMPEG_PREFIX_DIR "${CMAKE_CURRENT_BINARY_DIR}/FFmpeg-${_system_name}-${_system_processor}")
    set(FFMPEG_INSTALL_DIR "${FFMPEG_PREFIX_DIR}/install")
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
    if (NOT "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "${_system_processor}"
		OR NOT "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "${_system_name}")
        set(FFMPEG_TARGET_OS ${_system_name})
        set(FFMPEG_ENV "")
        if (MINGW)
            set(FFMPEG_TARGET_OS "mingw32")
            list(APPEND FFMPEG_CONFIGURE_ARGS
                --cross-prefix=${_system_processor}-w64-mingw32-
                --host-cc=${CMAKE_C_COMPILER} # HACK: FFmpeg complains about "lack of C11 support" without this because it can't find "gcc"
            )
        elseif(MSVC)
            set(FFMPEG_TARGET_OS "win32")
        elseif(APPLE)
            set(FFMPEG_TARGET_OS "darwin")
			set(FFMPEG_LDFLAGS "${CMAKE_C_FLAGS} -arch ${_system_processor} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -L${CMAKE_OSX_SYSROOT}/usr/lib -lSystem")
			set(FFMPEG_CFLAGS "${CMAKE_C_FLAGS} -arch ${_system_processor} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -w -I${CMAKE_OSX_SYSROOT}/usr/include")
            list(APPEND FFMPEG_CONFIGURE_ARGS
				--disable-asm
				--nm=${CMAKE_NM}
				--ar=${CMAKE_AR}
				--ranlib=${CMAKE_RANLIB}
				--cc=${CMAKE_C_COMPILER}
				--cxx=${CMAKE_CXX_COMPILER}
				--ld=${CMAKE_C_COMPILER}
				--strip=${CMAKE_STRIP}
                --host-cc=${CMAKE_C_COMPILER}
                --host-ld=${CMAKE_C_COMPILER}
				--extra-cflags=${FFMPEG_CFLAGS}
				--extra-cxxflags=${FFMPEG_CFLAGS}
				--extra-ldflags=${FFMPEG_LDFLAGS}
            )
        elseif(ANDROID)
            set(FFMPEG_TARGET_OS "android")

			# Get Android platform version (inexplicably apparently there literally isn't a better way to do this)
            string(REPLACE "-" " " _android_platform_args ${ANDROID_PLATFORM})
			separate_arguments(_android_platform_args)
			list(GET _android_platform_args 1 _android_ver)

			# Construct compiler
			if("${_system_processor}" STREQUAL "aarch64")
				set(_android_ffmpeg_cross_prefix "aarch64-linux-android${_android_ver}-")
			elseif("${_system_processor}" STREQUAL "armv7-a")
				set(_android_ffmpeg_cross_prefix "armv7a-linux-androideabi${_android_ver}-")
			elseif("${_system_processor}" STREQUAL "i686")
				set(_android_ffmpeg_cross_prefix "i686-linux-android${_android_ver}-")
			elseif("${_system_processor}" STREQUAL "x86_64")
				set(_android_ffmpeg_cross_prefix "x86_64-linux-android${_android_ver}-")
			else()
				message(FATAL_ERROR "Don't know how to compile FFmpeg on architecture ${_system_processor}")
			endif()

            list(APPEND FFMPEG_CONFIGURE_ARGS
				--cross-prefix=${_android_ffmpeg_cross_prefix}
				--host-cc=${_android_ffmpeg_cross_prefix}clang
				--host-ld=${_android_ffmpeg_cross_prefix}clang
                --extra-cflags=${CMAKE_C_FLAGS}
                --extra-ldflags=${CMAKE_SHARED_LINKER_FLAGS} # CMAKE_STATIC_LINKER_FLAGS is technically more appropriate but it's empty for some reason
                --sysroot=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/sysroot
                --ar=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-ar
                --ranlib=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-ranlib
                --disable-asm
                --nm=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-nm
                --strip=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin/llvm-strip
            )
            list(APPEND FFMPEG_ENV "PATH=$ENV{PATH}:${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-${CMAKE_HOST_SYSTEM_PROCESSOR}/bin:CFLAGS=${CMAKE_C_FLAGS}:LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}")
        elseif(LINUX)
            set(FFMPEG_TARGET_OS "linux")
            list(APPEND FFMPEG_CONFIGURE_ARGS
                --cross-prefix=${_system_processor}-linux-gnu-
                --host-cc=${CMAKE_C_COMPILER}
				--enable-vaapi
            )
        endif()
        list(APPEND FFMPEG_CONFIGURE_ARGS
            --enable-cross-compile
            --arch=${_system_processor}
            --target-os=${FFMPEG_TARGET_OS}
        )
    endif()

    set(FFMPEG_FILES "")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_INSTALL_DIR}/include")
    foreach(_component ${_ffmpeg_components})
        add_library("${_target_name}::${_component}" SHARED IMPORTED)
        if (MSVC)
            set(_component_lib "${FFMPEG_INSTALL_DIR}/bin/${_component}.lib")
        else()
            set(_component_lib "${FFMPEG_INSTALL_DIR}/lib/lib${_component}.a")
        endif()
        set_target_properties("${_target_name}::${_component}" PROPERTIES
            IMPORTED_IMPLIB ${_component_lib}
            IMPORTED_LOCATION ${_component_lib}
        )
        list(APPEND FFMPEG_FILES ${_component_lib})
        add_dependencies("${_target_name}::${_component}" ${_target_name})
    endforeach()

    list(APPEND FFMPEG_FILES ${FFMPEG_INCLUDE_DIR})
    include_directories(${FFMPEG_INCLUDE_DIR})

    ExternalProject_Add(${_target_name}
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
		PREFIX ${FFMPEG_PREFIX_DIR}
        GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
        GIT_TAG n7.1
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${FFMPEG_ENV} "../${_target_name}/configure" ${FFMPEG_CONFIGURE_ARGS}
        BUILD_COMMAND ${CMAKE_COMMAND} -E env ${FFMPEG_ENV} make -j${CMAKE_BUILD_PARALLEL_LEVEL}
        UPDATE_COMMAND ""
        BUILD_BYPRODUCTS
            ${FFMPEG_FILES}
    )
endfunction()

if (VANILLA_BUILD_VENDORED)
    # Static build
    if (NOT ANDROID)
        set(BUILD_SHARED_LIBS OFF)

        set(_sdl_lib SDL2::SDL2-static)
    endif()

    # Link zlib for PNG
    if (UNIX)
        list(APPEND _vanilla_gui_libs "z")
    endif()

    include(FetchContent)
    include(ExternalProject)

    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.32.8
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
    set(LIBXML2_WITH_TESTS OFF)
    FetchContent_Declare(
        libxml2
        GIT_REPOSITORY https://gitlab.gnome.org/GNOME/libxml2.git
        GIT_TAG v2.13.5
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(SDL2 SDL2_ttf SDL2_image libxml2)

	if (APPLE)
		# Compile FFmpeg for both x86_64 and arm64
		foreach (_arch ${CMAKE_OSX_ARCHITECTURES})
			compile_ffmpeg("FFmpeg_${_arch}" "${CMAKE_SYSTEM_NAME}" "${_arch}")
		endforeach()

		# lipo both binaries together
		foreach(_component ${_ffmpeg_components})
			# Determine which architectures we will "lipo" together
			set(_lipo_inputs "")
			set(_lipo_files "")
			foreach (_arch ${CMAKE_OSX_ARCHITECTURES})
				# Append target
				list(APPEND _lipo_inputs FFmpeg_${_arch}::${_component})

				# Append filename
				get_property(_lib_file TARGET FFmpeg_${_arch}::${_component} PROPERTY LOCATION)
				list(APPEND _lipo_files ${_lib_file})
			endforeach()

			# Create command to lipo architectures together
			set(_component_lib "${CMAKE_CURRENT_BINARY_DIR}/${_component}.a")
			add_custom_command(
				OUTPUT ${_component_lib}
				COMMAND lipo -create -output ${_component_lib} ${_lipo_files}
				DEPENDS ${_lipo_inputs}
			)

			# Add as static library
			add_custom_target("FFmpeg_${_component}" DEPENDS ${_component_lib})
			add_library("FFmpeg::${_component}" STATIC IMPORTED)
			add_dependencies("FFmpeg::${_component}" "FFmpeg_${_component}")
			set_target_properties("FFmpeg::${_component}" PROPERTIES
				IMPORTED_IMPLIB ${_component_lib}
				IMPORTED_LOCATION ${_component_lib}
			)
		endforeach()
	else()
		compile_ffmpeg("FFmpeg" "${CMAKE_SYSTEM_NAME}" "${CMAKE_SYSTEM_PROCESSOR}")
	endif()
endif()

list(APPEND VANILLA_GUI_SRC
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
    menu/menu_sudo.c
    menu/menu_sudo_warning.c
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
    add_library(vanilla SHARED ${VANILLA_GUI_SRC})
else()
    add_executable(vanilla MACOSX_BUNDLE ${VANILLA_GUI_SRC})
endif()

if (APPLE)
	# Set bundle name to be more user-friendly
	set_target_properties(vanilla PROPERTIES
		OUTPUT_NAME Vanilla
	)
else()
	# Ensure executable is installed
	install(TARGETS vanilla)
endif()

# Find packages (unless using vendored build)
if (NOT VANILLA_BUILD_VENDORED)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(SDL2_image REQUIRED)
endif()

target_link_libraries(vanilla PRIVATE
    ${_sdl_lib}
    SDL2::SDL2main
    SDL2_ttf
    SDL2_image
    libvanilla
    m
)

if (ANDROID)
    target_link_libraries(vanilla PRIVATE
        SDL2main
        log
    )
endif()

# Find FFmpeg
if (NOT VANILLA_BUILD_VENDORED)
    find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swscale)
endif()

target_link_libraries(vanilla PRIVATE
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    ${_vanilla_gui_libs}
)

if (APPLE)
	target_link_libraries(vanilla PRIVATE "-framework VideoToolbox" "-framework CoreMedia" iconv)
endif()

if (VANILLA_BUILD_PIPE)
	# Assume this platform is capable of running vanilla-pipe
    target_compile_definitions(vanilla PRIVATE VANILLA_PIPE_AVAILABLE)

	# Find Polkit and GLIB libraries
	find_package(PkgConfig)
	pkg_check_modules(PC_POLKIT polkit-agent-1 REQUIRED IMPORTED_TARGET)
	target_include_directories(vanilla PRIVATE ${PC_POLKIT_INCLUDE_DIRS})
	target_link_libraries(vanilla PRIVATE ${PC_POLKIT_LIBRARIES})
endif()

if (LINUX)
    # Link with DRM if available
    find_package(DRM)
    if (DRM_FOUND)
        target_link_libraries(vanilla PRIVATE ${DRM_LIBRARIES})
        target_include_directories(vanilla PRIVATE ${DRM_INCLUDE_DIRS})
	endif()

	# TODO: A more comprehensive "Find" for libva
	find_package(OpenGL COMPONENTS EGL)
	if (OpenGL_EGL_FOUND)
    	target_link_libraries(vanilla PRIVATE OpenGL::EGL va va-drm va-x11 X11)
		target_compile_definitions(vanilla PRIVATE VANILLA_HAS_EGL)
	endif()
endif()

# Find libxml2 (used for storing/parsing user config)
if (VANILLA_BUILD_VENDORED)
    target_link_libraries(vanilla PRIVATE LibXml2::LibXml2)
else()
    find_package(LibXML2 REQUIRED)
    target_link_libraries(vanilla PRIVATE ${LibXML2_LIBRARIES})
    target_include_directories(vanilla PRIVATE ${LibXML2_INCLUDE_DIR})
endif()


# Link with winsock2 on Windows
if (WIN32)
    set_target_properties(vanilla PROPERTIES WIN32_EXECUTABLE true)
    target_link_libraries(vanilla PRIVATE wsock32 ws2_32)
endif()

# Set up include directories around the repository
target_include_directories(vanilla PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# Install assets
if(ANDROID)
    # Put assets in correct place relative to build
    add_custom_command(
        TARGET vanilla POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_CURRENT_SOURCE_DIR}/../android/app/src/main/assets"
    )
elseif(APPLE)
	# Put assets in correct place relative to build
    add_custom_command(
        TARGET vanilla POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:vanilla>/../Resources/assets"
    )
else()
    # Put assets in correct place relative to build
    add_custom_command(
        TARGET vanilla POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../share/vanilla/assets"
    )

    if(WIN32)
        set(_vanilla_gui_asset_destination "bin")
    else()
        set(_vanilla_gui_asset_destination "share/vanilla")
    endif()
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${_vanilla_gui_asset_destination}")

    if (LINUX)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/com.mattkc.vanilla.desktop" DESTINATION share/applications)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/com.mattkc.vanilla.svg" DESTINATION share/pixmaps)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/com.mattkc.vanilla_rounded.svg" DESTINATION share/pixmaps)
    endif()
endif()
