cmake_minimum_required(VERSION 3.21)

project(Vanilla C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_C_STANDARD 23)

if(UNIX AND NOT APPLE AND NOT ANDROID)
    set(LINUX TRUE)
endif()

if(APPLE)
    # FFmpeg's configure step expects both compiler drivers even though Vanilla
    # itself is written in C.
    enable_language(CXX)
endif()

option(VANILLA_IOS_LEGACY "Build for iOS 12.0" OFF)
option(VANILLA_IOS_CODE_SIGNING "Enable automatic signing for iOS device builds" OFF)
set(VANILLA_IOS_DEVELOPMENT_TEAM "" CACHE STRING "Apple development team used to sign the iOS app")

if(IOS)
    if(NOT XCODE)
        message(FATAL_ERROR
            "iOS builds require the Xcode generator. Configure with one of the ios-* presets."
        )
    endif()

    string(TOLOWER "${CMAKE_OSX_SYSROOT}" _vanilla_ios_sysroot_lower)
    if(_vanilla_ios_sysroot_lower MATCHES "simulator")
        set(IOS_SIMULATOR TRUE)
    else()
        set(IOS_SIMULATOR FALSE)
    endif()

    list(LENGTH CMAKE_OSX_ARCHITECTURES _vanilla_ios_arch_count)
    if(NOT _vanilla_ios_arch_count EQUAL 1)
        message(FATAL_ERROR
            "iOS builds require exactly one CMAKE_OSX_ARCHITECTURES entry because "
            "the vendored FFmpeg build is configured for one architecture at a time."
        )
    endif()

    if(IS_DIRECTORY "${CMAKE_OSX_SYSROOT}")
        set(VANILLA_IOS_SDK_PATH "${CMAKE_OSX_SYSROOT}")
    else()
        execute_process(
            COMMAND xcrun --sdk "${CMAKE_OSX_SYSROOT}" --show-sdk-path
            RESULT_VARIABLE _vanilla_ios_sdk_result
            OUTPUT_VARIABLE VANILLA_IOS_SDK_PATH
            ERROR_VARIABLE _vanilla_ios_sdk_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT _vanilla_ios_sdk_result EQUAL 0 OR NOT IS_DIRECTORY "${VANILLA_IOS_SDK_PATH}")
            message(FATAL_ERROR
                "Could not resolve iOS SDK '${CMAKE_OSX_SYSROOT}': ${_vanilla_ios_sdk_error}"
            )
        endif()
    endif()
endif()

if(ANDROID OR MSVC OR IOS)
    set(vendored_default TRUE)
else()
    set(vendored_default FALSE)
endif()

set(vanilla_pipe_default OFF)
if(LINUX OR ANDROID)
    set(vanilla_pipe_default ON)
endif()

OPTION(VANILLA_BUILD_GUI "Build SDL2 GUI app" ON)
OPTION(VANILLA_BUILD_TESTS "Build unit tests for Vanilla" OFF)
OPTION(VANILLA_BUILD_PIPE "Build vanilla-pipe for connecting to Wii U" ${vanilla_pipe_default})
OPTION(VANILLA_BUILD_VENDORED "Build Vanilla with \"vendored\" third-party libraries" ${vendored_default})

add_subdirectory(lib)
if (VANILLA_BUILD_PIPE)
	add_subdirectory(pipe)
endif()
if (VANILLA_BUILD_GUI)
    add_subdirectory(gui)
endif()
