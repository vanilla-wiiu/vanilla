# - Try to find LibXML2 headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(LibXML2)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LibXML2_ROOT_DIR  Set this variable to the root installation of
#                    LibXML2 if the module has problems finding
#                    the proper installation path.
#
# Variables defined by this module:
#
#  LIBXML2_FOUND              System has LibXML2 libs/headers
#  LibXML2_LIBRARIES          The LibXML2 libraries
#  LibXML2_INCLUDE_DIR        The location of LibXML2 headers

find_path(LibXML2_ROOT_DIR
    NAMES include/libxml2/libxml/tree.h
)

find_library(LibXML2_LIBRARIES
    NAMES xml2
    HINTS ${LibXML2_ROOT_DIR}/lib
)

find_path(LibXML2_INCLUDE_DIR
    NAMES libxml/tree.h
    HINTS ${LibXML2_ROOT_DIR}/include/libxml2
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibXML2 DEFAULT_MSG
    LibXML2_LIBRARIES
    LibXML2_INCLUDE_DIR
)

mark_as_advanced(
    LibXML2_ROOT_DIR
    LibXML2_LIBRARIES
    LibXML2_INCLUDE_DIR
)
