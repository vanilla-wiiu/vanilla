include(ExternalProject)

# Add vanilla-pipe executable
add_executable(vanilla-pipe
    dhcp/dhcp.c
    main.c
    wpa.c
)

if(ANDROID)
    target_sources(vanilla-pipe PRIVATE android.c)
endif()

# Install vanilla-pipe
install(TARGETS vanilla-pipe)

# vanilla-pipe remains an executable, but is given a .so extension so Gradle will
# place it alongside main executable libvanilla.so
if(ANDROID)
	set_target_properties(vanilla-pipe PROPERTIES
		OUTPUT_NAME vanilla_pipe
		PREFIX "lib"
		SUFFIX ".so"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
	)
endif()

option(VANILLA_BUILD_USE_LIBNM "Build with support for communicating to NetworkManager" ${LINUX})
if (VANILLA_BUILD_USE_LIBNM)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBNM REQUIRED libnm)
	target_link_libraries(vanilla-pipe PRIVATE ${LIBNM_LIBRARIES})
	target_include_directories(vanilla-pipe PRIVATE ${LIBNM_INCLUDE_DIRS})
	target_compile_definitions(vanilla-pipe PRIVATE USE_LIBNM)
endif()

set(PIPE_TOOLCHAIN_FLAGS "")
if(ANDROID)
	set(PIPE_TOOLCHAIN_FLAGS
		"--target=${ANDROID_LLVM_TRIPLE} --sysroot=${CMAKE_SYSROOT}"
	)
endif()

if (VANILLA_BUILD_VENDORED)
	# Build LibNL
	set(LIBNL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libnl)
	set(LIBNL_INSTALL_DIR ${LIBNL_DIR}/install)
	set(LIBNL_EXTRA_FLAGS "")
	set(LIBNL_CFLAGS "${CMAKE_C_FLAGS} ${PIPE_TOOLCHAIN_FLAGS}")
	if(ANDROID)
		set(LIBNL_EXTRA_FLAGS --disable-pthreads)
		string(APPEND LIBNL_CFLAGS " -D__UAPI_DEF_IN_ADDR=0 -include bits/in_addr.h")
	endif()

    if (ANDROID)
		set(LIBNL_HOST ${ANDROID_LLVM_TRIPLE})
	elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")
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
		PATCH_COMMAND autoreconf -i
		CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
			CC=${CMAKE_C_COMPILER}
			AR=${CMAKE_AR}
			RANLIB=${CMAKE_RANLIB}
			"CFLAGS=${LIBNL_CFLAGS}"
			"LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}"
			./configure --prefix=${LIBNL_INSTALL_DIR} --host=${LIBNL_HOST} --disable-shared --enable-static --disable-cli ${LIBNL_EXTRA_FLAGS}
		UPDATE_COMMAND ""
		BUILD_BYPRODUCTS
			${LibNL_LIBRARY}
			${LibNL_ROUTE_LIBRARY}
			${LibNL_GENL_LIBRARY}
			${LibNL_INCLUDE_DIR}
	)
	add_dependencies(vanilla-pipe libnl)
else()
	# Find system LibNL
	find_package(LibNL REQUIRED)
endif()

# Build WPA supplicant
set(HOSTAP_DIR ${CMAKE_CURRENT_BINARY_DIR}/hostap)
set(HOSTAP_CFLAGS "${PIPE_TOOLCHAIN_FLAGS}")
set(HOSTAP_ENV
	CC=${CMAKE_C_COMPILER}
	AR=${CMAKE_AR}
	RANLIB=${CMAKE_RANLIB}
	CONFIG_TLS=internal
	CONFIG_CRYPTO=internal
	CONFIG_INTERNAL_LIBTOMMATH=y
	CONFIG_INTERNAL_LIBTOMMATH_FAST=y
)
if (VANILLA_BUILD_VENDORED)
	string(APPEND HOSTAP_CFLAGS " -I${LIBNL_INSTALL_DIR}/include/libnl3")
	set(HOSTAP_LDFLAGS "-L${LIBNL_INSTALL_DIR}/lib")
	list(APPEND HOSTAP_ENV
		PKG_CONFIG_PATH=${LIBNL_INSTALL_DIR}/lib/pkgconfig
		LDFLAGS=${HOSTAP_LDFLAGS}
	)
endif()
if(HOSTAP_CFLAGS)
	list(APPEND HOSTAP_ENV "EXTRA_CFLAGS=${HOSTAP_CFLAGS}")
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
        ${CMAKE_COMMAND} -E env --unset=GIT_DIR ${GIT_EXECUTABLE} clone https://github.com/vanilla-wiiu/drc-hostap.git "${HOSTAP_DIR}" &&
        ${CMAKE_COMMAND} -E env --unset=GIT_DIR ${GIT_EXECUTABLE} -C "${HOSTAP_DIR}" checkout a2f705daa143791180754100194c91714f7e8801
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_if_different conf/wpa_supplicant.config wpa_supplicant/.config
    BUILD_COMMAND env ${HOSTAP_ENV} make -C wpa_supplicant clean &&
        env ${HOSTAP_ENV} make -C wpa_supplicant -j${CMAKE_BUILD_PARALLEL_LEVEL} libwpa_supplicant.a libwpa_client.a
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_BYPRODUCTS
        ${HOSTAP_DIR}/wpa_supplicant/libwpa_supplicant.a
        ${HOSTAP_DIR}/wpa_supplicant/libwpa_client.a
)
if (VANILLA_BUILD_VENDORED)
	add_dependencies(hostap libnl)
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

# Define vanilla-pipe include directories
target_include_directories(vanilla-pipe PRIVATE
    ${HOSTAP_DIR}/src
    ${HOSTAP_DIR}/src/common
    ${HOSTAP_DIR}/wpa_supplicant
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib
    ${LibNL_INCLUDE_DIR}
)

# Link our library with the client library
target_link_libraries(vanilla-pipe PRIVATE
    wpa_client
    wpa_supplicant_drc
    ${LibNL_LIBRARY}
    ${LibNL_ROUTE_LIBRARY}
    ${LibNL_GENL_LIBRARY}
	${CMAKE_DL_LIBS}
	m
)
if(NOT ANDROID)
	target_link_libraries(vanilla-pipe PRIVATE pthread)
endif()
