cmake_minimum_required(VERSION 3.6)

project(GAME)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()

# Android has no need for a "windowed" option
set(VANILLA_GUI_ENABLE_WINDOWED OFF)

# Add source files and keep in individual per-ABI dirs
add_subdirectory(../../.. "${CMAKE_CURRENT_BINARY_DIR}/vanilla")
