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

# Detect iOS (set by toolchain file or CMake)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IOS TRUE)
endif()

if(ANDROID OR MSVC OR IOS)
    set(vendored_default TRUE)
else()
    set(vendored_default FALSE)
endif()

OPTION(VANILLA_BUILD_GUI "Build SDL2 GUI app" ON)
OPTION(VANILLA_BUILD_TESTS "Build unit tests for Vanilla" OFF)
OPTION(VANILLA_BUILD_PIPE "Build vanilla-pipe for connecting to Wii U (Linux only)" ${LINUX})
OPTION(VANILLA_BUILD_VENDORED "Build Vanilla with \"vendored\" third-party libraries" ${vendored_default})

add_subdirectory(lib)
if (VANILLA_BUILD_PIPE)
	add_subdirectory(pipe)
endif()
if (VANILLA_BUILD_GUI)
    add_subdirectory(gui)
endif()
