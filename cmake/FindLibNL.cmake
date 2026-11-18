find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBNL QUIET libnl-3.0)
  pkg_check_modules(PC_LIBNL_ROUTE QUIET libnl-route-3.0)
  pkg_check_modules(PC_LIBNL_NETFILTER QUIET libnl-nf-3.0)
  pkg_check_modules(PC_LIBNL_GENL QUIET libnl-genl-3.0)
endif()

find_path(LibNL_INCLUDE_DIR
  NAMES netlink/netlink.h
  HINTS ${PC_LIBNL_INCLUDE_DIRS}
  PATH_SUFFIXES include/libnl3 libnl3
)

find_library(LibNL_LIBRARY
  NAMES nl-3 nl
  HINTS ${PC_LIBNL_LIBRARY_DIRS}
)
find_library(LibNL_ROUTE_LIBRARY
  NAMES nl-route-3 nl-route
  HINTS ${PC_LIBNL_ROUTE_LIBRARY_DIRS}
)
find_library(LibNL_NETFILTER_LIBRARY
  NAMES nl-nf-3 nl-nf
  HINTS ${PC_LIBNL_NETFILTER_LIBRARY_DIRS}
)
find_library(LibNL_GENL_LIBRARY
  NAMES nl-genl-3 nl-genl
  HINTS ${PC_LIBNL_GENL_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibNL DEFAULT_MSG
  LibNL_INCLUDE_DIR
  LibNL_LIBRARY
  LibNL_ROUTE_LIBRARY
  LibNL_GENL_LIBRARY
)
