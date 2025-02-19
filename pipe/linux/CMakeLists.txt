# Add vanilla-pipe executalbe
add_executable(vanilla-pipe
    main.c
    wpa.c
)

# Install vanilla-pipe
install(TARGETS vanilla-pipe)

# Build WPA supplicant
include(ExternalProject)
set(HOSTAP_DIR ${CMAKE_CURRENT_BINARY_DIR}/hostap)
ExternalProject_Add(hostap
    SOURCE_DIR ${HOSTAP_DIR}
    BINARY_DIR ${HOSTAP_DIR}
    GIT_REPOSITORY https://github.com/rolandoislas/drc-hostap.git
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_if_different conf/wpa_supplicant.config wpa_supplicant/.config
    BUILD_COMMAND make -C wpa_supplicant wpa_supplicant libwpa_client.a && cp wpa_supplicant/wpa_supplicant wpa_supplicant/wpa_supplicant_drc
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_BYPRODUCTS
        ${HOSTAP_DIR}/wpa_supplicant/wpa_supplicant_drc
        ${HOSTAP_DIR}/wpa_supplicant/libwpa_client.a
)

# Define wpa_client library from hostap
add_library(wpa_client SHARED IMPORTED)
set_target_properties(wpa_client PROPERTIES
    IMPORTED_IMPLIB ${HOSTAP_DIR}/wpa_supplicant/libwpa_client.a
    IMPORTED_NO_SONAME true
)
add_dependencies(wpa_client hostap)

# Define wpa_supplicant binary from hostap
add_executable(wpa_supplicant IMPORTED)
set_target_properties(wpa_supplicant PROPERTIES
    IMPORTED_LOCATION ${HOSTAP_DIR}/wpa_supplicant/wpa_supplicant_drc
)
add_dependencies(wpa_supplicant hostap)
add_custom_command(
    TARGET hostap POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HOSTAP_DIR}/wpa_supplicant/wpa_supplicant_drc" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/wpa_supplicant_drc"
)

# Compile custom dhclient library
add_subdirectory(dhcp)

# Find LibNL
find_package(LibNL REQUIRED)

# Define vanilla-pipe include directories
target_include_directories(vanilla-pipe PRIVATE
    ${HOSTAP_DIR}/src/common
    ${HOSTAP_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/lib
    ${LibNL_INCLUDE_DIR}
)

# Link our library with the client library
target_link_libraries(vanilla-pipe PRIVATE
    wpa_client
    pthread
    udhcpc
    ${LibNL_LIBRARY}
    ${LibNL_ROUTE_LIBRARY}
)

# Ensure wpa_supplicant is installed
install(IMPORTED_RUNTIME_ARTIFACTS wpa_supplicant)

# Handle polkit files
if (LINUX)
    install(FILES "polkit/vanilla-polkit-install.sh" DESTINATION bin)
endif()
