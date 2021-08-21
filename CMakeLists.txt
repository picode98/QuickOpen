# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# set(wxWidgets_ROOT_DIR ${VCPKG_ROOT}/installed/x64-windows)

# if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
#      CACHE STRING "")
# endif()

project ("QuickOpen")

# Include sub-projects.
add_subdirectory ("QuickOpenCommon")