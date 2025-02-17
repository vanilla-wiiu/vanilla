cmake_minimum_required(VERSION 3.26)

add_custom_command(
  TARGET vanilla-pi
  POST_BUILD
  COMMAND ${CMAKE_COMMAND}
  ARGS -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/../../folderA" "${PROJECT_SOURCE_DIR}/release/Debug/${PROJECT_NAME}.app/Contents/"
  COMMENT "Copying Assets"
)
