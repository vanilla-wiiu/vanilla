cmake_minimum_required(VERSION 3.21)

project(Vanilla C CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/finders")

add_subdirectory(lib)
add_subdirectory(pipe)
add_subdirectory(app)
