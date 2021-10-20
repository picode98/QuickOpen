# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.9)

# set(wxWidgets_ROOT_DIR ${VCPKG_ROOT}/installed/x64-windows)

# if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
#      CACHE STRING "")
# endif()

file(STRINGS "QuickOpen/ApplicationInfo.h" lineList)
foreach(thisLine lineList)
    if(thisLine MATCHES "^#define QUICKOPEN_VERSION_STR \"(.*)\"$")
        set(headerVersion CMAKE_MATCH_1)
    endif()
endforeach()

if(DEFINED headerVersion)
    project ("QuickOpen" VERSION ${headerVersion})
    message(STATUS "Set version to ${headerVersion}.")
else()
    project ("QuickOpen")
    message(WARNING "Could not identify project version from header file.")
endif()

# Include sub-projects.
add_subdirectory ("QuickOpen")