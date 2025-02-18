#.rst:
# FindDRM
# -------
#
# Find DRM library
#
# Try to find DRM library on UNIX systems. The following values are defined
#
# ::
#
#   DRM_FOUND         - True if drm is available
#   DRM_INCLUDE_DIRS  - Include directories for drm
#   DRM_LIBRARIES     - List of libraries for drm
#   DRM_DEFINITIONS   - List of definitions for drm
#
#=============================================================================
# Copyright (c) 2015 Jari Vetoniemi
#
# Distributed under the OSI-approved BSD License (the "License");
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

include(FeatureSummary)
set_package_properties(DRM PROPERTIES
   URL "http://dri.freedesktop.org/"
   DESCRIPTION "Kernel module that gives direct hardware access to DRI clients")

find_package(PkgConfig)
pkg_check_modules(PC_DRM QUIET libdrm)
find_library(DRM_LIBRARIES NAMES drm HINTS ${PC_DRM_LIBRARY_DIRS})
find_path(DRM_INCLUDE_DIRS NAMES drm.h PATH_SUFFIXES libdrm drm HINTS ${PC_DRM_INCLUDE_DIRS})

set(DRM_DEFINITIONS ${PC_DRM_CFLAGS_OTHER})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DRM DEFAULT_MSG DRM_INCLUDE_DIRS DRM_LIBRARIES)
mark_as_advanced(DRM_INCLUDE_DIRS DRM_LIBRARIES DRM_DEFINITIONS)