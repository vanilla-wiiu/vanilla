include(ExternalProject)

# Add vanilla-pipe executalbe
add_executable(vanilla-pipe
    main.c
    wpa.c
)

# Install vanilla-pipe
install(TARGETS vanilla-pipe)

option(VANILLA_BUILD_USE_LIBNM "Build with support for communicating to NetworkManager" ON)
if (VANILLA_BUILD_USE_LIBNM)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBNM REQUIRED libnm)
	target_link_libraries(vanilla-pipe PRIVATE ${LIBNM_LIBRARIES})
	target_include_directories(vanilla-pipe PRIVATE ${LIBNM_INCLUDE_DIRS})
	target_compile_definitions(vanilla-pipe PRIVATE USE_LIBNM)
endif()

if (VANILLA_BUILD_VENDORED)
	# Build OpenSSL
	set(OPENSSL_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl)
	set(OPENSSL_INSTALL_DIR ${OPENSSL_DIR}/install)
	set(OPENSSL_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
	set(OPENSSL_EXTRA_FLAGS "")
	if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")
		set(OPENSSL_PROCESSOR "armv4")
		set(OPENSSL_EXTRA_FLAGS "${CMAKE_C_FLAGS}")
	endif()
	set(OPENSSL_LIBRARIES "${OPENSSL_DIR}/libssl.a" "${OPENSSL_DIR}/libcrypto.a")
	ExternalProject_Add(openssl
		SOURCE_DIR ${OPENSSL_DIR}
		BINARY_DIR ${OPENSSL_DIR}
		GIT_REPOSITORY https://github.com/openssl/openssl.git
		GIT_TAG openssl-3.5.0
		CONFIGURE_COMMAND env CC=${CMAKE_C_COMPILER} ./Configure --prefix=${OPENSSL_INSTALL_DIR} linux-${OPENSSL_PROCESSOR}
		INSTALL_COMMAND make install_sw
		UPDATE_COMMAND ""
		BUILD_BYPRODUCTS
			${OPENSSL_LIBRARIES}
	)
	add_dependencies(vanilla-pipe openssl)

	# Build LibNL
	set(LIBNL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libnl)
	set(LIBNL_INSTALL_DIR ${LIBNL_DIR}/install)
	if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")
		set(LIBNL_HOST arm-linux-gnueabihf)
	else()
		set(LIBNL_HOST ${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
	endif()
	set(LibNL_LIBRARY "${LIBNL_INSTALL_DIR}/lib/libnl-3.a")
	set(LibNL_ROUTE_LIBRARY "${LIBNL_INSTALL_DIR}/lib/libnl-route-3.a")
	set(LibNL_GENL_LIBRARY "${LIBNL_INSTALL_DIR}/lib/libnl-genl-3.a")
	set(LibNL_INCLUDE_DIR "${LIBNL_INSTALL_DIR}/include/libnl3")
	ExternalProject_Add(libnl
		SOURCE_DIR ${LIBNL_DIR}
		BINARY_DIR ${LIBNL_DIR}
		GIT_REPOSITORY https://github.com/thom311/libnl.git
		GIT_TAG libnl3_11_0
		CONFIGURE_COMMAND autoreconf -i && ./configure --prefix=${LIBNL_INSTALL_DIR} --host=${LIBNL_HOST}
		UPDATE_COMMAND ""
		BUILD_BYPRODUCTS
			${LibNL_LIBRARY}
			${LibNL_ROUTE_LIBRARY}
			${LibNL_GENL_LIBRARY}
			${LibNL_INCLUDE_DIR}
	)
	add_dependencies(vanilla-pipe libnl)
else()
	# Find LibNL
	find_package(LibNL REQUIRED)

	# Find OpenSSL and libnm
	find_package(OpenSSL REQUIRED)
endif()

# Build WPA supplicant
set(HOSTAP_DIR ${CMAKE_CURRENT_BINARY_DIR}/hostap)
set(HOSTAP_ENV
	CC=${CMAKE_C_COMPILER}
)
if (VANILLA_BUILD_VENDORED)
	set(HOSTAP_CFLAGS "-I${LIBNL_INSTALL_DIR}/include/libnl3 -I${OPENSSL_INSTALL_DIR}/include")
	set(HOSTAP_LDFLAGS "-L${LIBNL_INSTALL_DIR}/lib -L${OPENSSL_INSTALL_DIR}/lib64 -L${OPENSSL_INSTALL_DIR}/lib")
	list(APPEND HOSTAP_ENV
		PKG_CONFIG_PATH=${LIBNL_INSTALL_DIR}/lib/pkgconfig
		EXTRA_CFLAGS=${HOSTAP_CFLAGS}
		LDFLAGS=${HOSTAP_LDFLAGS}
	)
endif()
find_package(Git REQUIRED)
ExternalProject_Add(hostap
    SOURCE_DIR ${HOSTAP_DIR}
    BINARY_DIR ${HOSTAP_DIR}

    # HACK: Kind of silly that we have to do this, but Buildroot insists on
    #       setting GIT_DIR=. which fucks this up. So we have to write our own
    #       DOWNLOAD_COMMAND which explicitly unsets GIT_DIR to solve that.
    #GIT_REPOSITORY https://github.com/vanilla-wiiu/drc-hostap.git
    DOWNLOAD_COMMAND
        ${CMAKE_COMMAND} -E remove_directory "${HOSTAP_DIR}" &&
        ${CMAKE_COMMAND} -E env GIT_DIR=
        ${GIT_EXECUTABLE} clone https://github.com/vanilla-wiiu/drc-hostap.git "${HOSTAP_DIR}" &&
        ${GIT_EXECUTABLE} -C "${HOSTAP_DIR}" checkout af1da944765c6d04002584ea0d70c43899a5e6ea
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_if_different conf/wpa_supplicant.config wpa_supplicant/.config
    BUILD_COMMAND env ${HOSTAP_ENV} make -C wpa_supplicant -j${CMAKE_BUILD_PARALLEL_LEVEL} libwpa_supplicant.a libwpa_client.a
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_BYPRODUCTS
        ${HOSTAP_DIR}/wpa_supplicant/libwpa_supplicant.a
        ${HOSTAP_DIR}/wpa_supplicant/libwpa_client.a
)
if (VANILLA_BUILD_VENDORED)
	add_dependencies(hostap openssl libnl)
endif()

# Define wpa_client library from hostap
add_library(wpa_client STATIC IMPORTED)
set_target_properties(wpa_client PROPERTIES
    IMPORTED_LOCATION ${HOSTAP_DIR}/wpa_supplicant/libwpa_client.a
    IMPORTED_NO_SONAME true
)
add_dependencies(wpa_client hostap)

# Define wpa_supplicant binary from hostap
add_library(wpa_supplicant_drc STATIC IMPORTED)
set_target_properties(wpa_supplicant_drc PROPERTIES
    IMPORTED_LOCATION ${HOSTAP_DIR}/wpa_supplicant/libwpa_supplicant.a
    IMPORTED_NO_SONAME true
)
add_dependencies(wpa_supplicant_drc hostap)

# Compile custom dhclient library
add_subdirectory(dhcp EXCLUDE_FROM_ALL)

# Define vanilla-pipe include directories
target_include_directories(vanilla-pipe PRIVATE
    ${HOSTAP_DIR}/src
    ${HOSTAP_DIR}/src/common
    ${HOSTAP_DIR}/wpa_supplicant
    ${CMAKE_SOURCE_DIR}/lib
    ${LibNL_INCLUDE_DIR}
)

# Link our library with the client library
target_link_libraries(vanilla-pipe PRIVATE
    wpa_client
    pthread
    udhcpc
    wpa_supplicant_drc
    ${LibNL_LIBRARY}
    ${LibNL_ROUTE_LIBRARY}
    ${LibNL_GENL_LIBRARY}
    ${OPENSSL_LIBRARIES}
)
