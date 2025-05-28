cmake_minimum_required(VERSION 3.6)

project(GAME)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()

# Your game and its CMakeLists.txt are in a subfolder named "src"
add_subdirectory(../../.. "${CMAKE_SOURCE_DIR}/build")
