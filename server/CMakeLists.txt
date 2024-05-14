add_library(vanilla SHARED
    gamepad.c
    server.c
    status.c
    sync.c
    util.c
    wpa.c
)

add_executable(vanilla-server
    main.c
)

target_link_libraries(vanilla-server PRIVATE
    vanilla
)

target_include_directories(vanilla PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hostap/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/hostap/src/utils
)

# Build WPA supplicant
add_custom_target(
    wpa_supplicant
    COMMAND make
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant"
)

# Build client library
add_custom_target(
    wpa_client_lib
    COMMAND make libwpa_client.so
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant"
    DEPENDS wpa_supplicant
)

# Make vanillaserver depend on client library
add_dependencies(vanilla wpa_client_lib)

# Link wpa_client
target_link_directories(vanilla PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant"
)

target_link_libraries(vanilla PRIVATE
    wpa_client
)

# Copy files into build directory
add_custom_command(
    TARGET wpa_supplicant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant/wpa_supplicant" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/wpa_supplicant_drc"
)