find_path(LibNL_INCLUDE_DIR
  NAMES netlink/netlink.h
  PATH_SUFFIXES include/libnl3 libnl3
)

find_library(LibNL_LIBRARY        NAMES nl-3 nl)
find_library(LibNL_ROUTE_LIBRARY  NAMES nl-route-3 nl-route)
find_library(LibNL_NETFILTER_LIBRARY NAMES nl-nf-3 nl-nf)
find_library(LibNL_GENL_LIBRARY   NAMES nl-genl-3 nl-genl)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibNL DEFAULT_MSG
  LibNL_INCLUDE_DIR
  LibNL_LIBRARY
  LibNL_ROUTE_LIBRARY
  LibNL_GENL_LIBRARY
)
