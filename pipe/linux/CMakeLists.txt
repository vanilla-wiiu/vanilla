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
#
# HACK: To skip having to call externally into wpa_supplicant, and since it has
#       no static library build path out of the box, we just build the objects
#       straight in and use it like a static library anyway. It's a little hacky
#       to list all of the expected build objects, but hey it works.
set(HOSTAP_DIR ${CMAKE_CURRENT_BINARY_DIR}/hostap)
set(WPA_SUPPLICANT_OBJECTS
    wpa_supplicant/wpa_supplicant.o
    wpa_supplicant/ctrl_iface.o
    wpa_supplicant/scan.o
    wpa_supplicant/events.o
    wpa_supplicant/config.o
    wpa_supplicant/sme.o
    wpa_supplicant/wpas_glue.o
    wpa_supplicant/wps_supplicant.o
    wpa_supplicant/ctrl_iface_unix.o
    wpa_supplicant/config_file.o
    wpa_supplicant/bss.o
    wpa_supplicant/blacklist.o
    wpa_supplicant/wmm_ac.o
    wpa_supplicant/notify.o
    wpa_supplicant/eap_register.o
    src/wps/wps_registrar.o
    src/wps/wps_enrollee.o
    src/wps/wps_common.o
    src/wps/wps_attr_build.o
    src/wps/wps_attr_parse.o
    src/wps/wps_dev_attr.o
    src/wps/wps.o
    src/wps/wps_attr_process.o
    src/utils/radiotap.o
    src/utils/common.o
    src/utils/base64.o
    src/utils/eloop.o
    src/utils/wpa_debug.o
    src/utils/os_unix.o
    src/utils/wpabuf.o
    src/utils/uuid.o
    src/rsn_supp/wpa.o
    src/rsn_supp/peerkey.o
    src/rsn_supp/pmksa_cache.o
    src/rsn_supp/wpa_ie.o
    src/rsn_supp/preauth.o
    src/l2_packet/l2_packet_linux.o
    src/eapol_supp/eapol_supp_sm.o
    src/eap_peer/eap.o
    src/eap_peer/eap_tls_common.o
    src/eap_peer/eap_ttls.o
    src/eap_peer/eap_peap.o
    src/eap_peer/eap_methods.o
    src/eap_peer/eap_wsc.o
    src/eap_peer/eap_mschapv2.o
    src/eap_peer/eap_tls.o
    src/eap_peer/eap_leap.o
    src/eap_peer/eap_gtc.o
    src/eap_peer/mschapv2.o
    src/eap_peer/eap_otp.o
    src/eap_peer/eap_md5.o
    src/eap_common/eap_common.o
    src/eap_common/chap.o
    src/eap_common/eap_wsc_common.o
    src/eap_common/eap_peap_common.o
    src/drivers/driver_nl80211.o
    src/drivers/driver_wext.o
    src/drivers/driver_nl80211_capa.o
    src/drivers/driver_nl80211_event.o
    src/drivers/driver_nl80211_scan.o
    src/drivers/rfkill.o
    src/drivers/driver_wired.o
    src/drivers/linux_ioctl.o
    src/drivers/driver_nl80211_monitor.o
    src/drivers/drivers.o
    src/drivers/netlink.o
    src/drivers/driver_common.o
    src/crypto/tls_openssl.o
    src/crypto/crypto_openssl.o
    src/crypto/sha1-prf.o
    src/crypto/random.o
    src/crypto/sha1-tlsprf.o
    src/crypto/tls_openssl_ocsp.o
    src/crypto/dh_groups.o
    src/crypto/sha256-prf.o
    src/crypto/sha256-tlsprf.o
    src/crypto/ms_funcs.o
    src/common/wpa_ctrl.o
    src/common/ieee802_11_common.o
    src/common/hw_features_common.o
    src/common/wpa_common.o
    src/common/ctrl_iface_common.o
)
set(WPA_SUPPLICANT_IMPORTED_OBJECTS "")
set(WPA_SUPPLICANT_BYPRODUCTS "")
foreach(_wpa_supplicant_obj ${WPA_SUPPLICANT_OBJECTS})
    set(_wpa_supplicant_obj_path "${HOSTAP_DIR}/${_wpa_supplicant_obj}")
    string(CONCAT WPA_SUPPLICANT_IMPORTED_OBJECTS "${WPA_SUPPLICANT_IMPORTED_OBJECTS}${_wpa_supplicant_obj_path}\;")
    list(APPEND WPA_SUPPLICANT_BYPRODUCTS "${_wpa_supplicant_obj_path}")
endforeach()

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
ExternalProject_Add(hostap
    SOURCE_DIR ${HOSTAP_DIR}
    BINARY_DIR ${HOSTAP_DIR}

    # HACK: Kind of silly that we have to do this, but Buildroot insists on
    #       setting GIT_DIR=. which fucks this up. So we have to write our own
    #       DOWNLOAD_COMMAND which explicitly unsets GIT_DIR to solve that.
    #GIT_REPOSITORY https://github.com/rolandoislas/drc-hostap.git
    DOWNLOAD_COMMAND
        ${CMAKE_COMMAND} -E env GIT_DIR=
        ${GIT_EXECUTABLE} clone --branch master --depth 1 https://github.com/rolandoislas/drc-hostap.git "${HOSTAP_DIR}"
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_if_different conf/wpa_supplicant.config wpa_supplicant/.config
    BUILD_COMMAND env ${HOSTAP_ENV} make -C wpa_supplicant -j${CMAKE_BUILD_PARALLEL_LEVEL} wpa_supplicant libwpa_client.a
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_BYPRODUCTS
        ${WPA_SUPPLICANT_BYPRODUCTS}
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
add_library(wpa_supplicant_drc OBJECT IMPORTED)
set_target_properties(wpa_supplicant_drc PROPERTIES IMPORTED_OBJECTS ${WPA_SUPPLICANT_IMPORTED_OBJECTS})
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
    ${LibNL_LIBRARY}
    ${LibNL_ROUTE_LIBRARY}
    ${LibNL_GENL_LIBRARY}
    ${OPENSSL_LIBRARIES}
    wpa_supplicant_drc
)
