add_library(vanillaserver SHARED
    server.c
    status.c
)

add_executable(vanillaserver_cli
    main.c
)

target_link_libraries(vanillaserver_cli PRIVATE
    vanillaserver
)

target_include_directories(vanillaserver PRIVATE
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
add_dependencies(vanillaserver wpa_client_lib)

# Link wpa_client
target_link_directories(vanillaserver PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant"
)

target_link_libraries(vanillaserver PRIVATE
    wpa_client
)

# Copy files into build directory
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant/wpa_supplicant" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/wpa_supplicant_drc" COPYONLY)