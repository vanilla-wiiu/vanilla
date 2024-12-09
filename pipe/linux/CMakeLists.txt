# Set up vanilla-pipe targer
add_executable(vanilla-pipe
    main.c
    wpa.c
)

install(TARGETS vanilla-pipe)

# Find libnm
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED libssl libcrypto)
pkg_check_modules(LIBNM REQUIRED libnm)
pkg_check_modules(LIBNL3 REQUIRED libnl-3.0 libnl-route-3.0 libnl-genl-3.0)

# Add include dirs for vanilla-pipe
target_include_directories(vanilla-pipe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hostap/src
    ${CMAKE_CURRENT_SOURCE_DIR}/hostap/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant
    ${CMAKE_SOURCE_DIR}/lib
    ${LIBNM_INCLUDE_DIRS}
    ${LIBNL3_INCLUDE_DIRS}
)

# Ensure submodules are checked out before we build anything
find_package(Git)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Build WPA supplicant
add_custom_target(
    wpa_supplicant_configure
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/hostap/conf/wpa_supplicant.config" "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant/.config"
)
add_custom_target(
    wpa_supplicant_build
    COMMAND make -j$$(nproc) libwpa_supplicant.a
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant"
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant/wpa_supplicant"
    USES_TERMINAL
    DEPENDS wpa_supplicant_configure
)
add_library(wpa_supplicant STATIC IMPORTED)
set_target_properties(wpa_supplicant PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant/libwpa_supplicant.a"
)
add_dependencies(wpa_supplicant wpa_supplicant_build)

# Build udhcpc (from busybox)
add_custom_target(
    udhcpc_configure
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/busybox.config" "${CMAKE_CURRENT_SOURCE_DIR}/busybox/.config"
)
add_custom_target(
    udhcpc_build
    COMMAND make -j$$(nproc)
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/busybox"
    USES_TERMINAL
    DEPENDS udhcpc_configure
)
add_library(udhcpc SHARED IMPORTED)
set_target_properties(udhcpc PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/busybox/0_lib/libbusybox.so.1.38.0.git"
)
install(IMPORTED_RUNTIME_ARTIFACTS udhcpc)

# Build client library
add_custom_target(
    wpa_client_build
    COMMAND make libwpa_client.a
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant"
    DEPENDS wpa_supplicant
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant/libwpa_client.a"
)
add_library(wpa_client STATIC IMPORTED)
set_target_properties(wpa_client PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/hostap/wpa_supplicant/libwpa_client.a"
)
add_dependencies(wpa_client wpa_client_build)

# Link our library with the client library
target_link_libraries(vanilla-pipe PRIVATE
    wpa_client
    pthread
    wpa_supplicant
    udhcpc
    ${LIBNM_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${LIBNL3_LIBRARIES}
)
