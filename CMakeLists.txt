# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.20)

if(WIN32)
    set(CPACK_GENERATOR "ZIP;WIX")
elseif(UNIX)
    set(CPACK_GENERATOR "ZIP;DEB")
else()
    set(CPACK_GENERATOR "ZIP")
endif()

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/pkg")
set(CPACK_PACKAGE_EXECUTABLES "QuickOpen;QuickOpen")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

set(CPACK_WIX_PATCH_FILE "${CMAKE_SOURCE_DIR}/MSICustomActions.xml")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/debian/postinst;${CMAKE_SOURCE_DIR}/debian/prerm;${CMAKE_SOURCE_DIR}/debian/copyright")

# set(wxWidgets_ROOT_DIR ${VCPKG_ROOT}/installed/x64-windows)

# if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
#      CACHE STRING "")
# endif()

file(STRINGS "QuickOpen/ApplicationInfo.h" lineList)
foreach(thisLine ${lineList})
    if(thisLine MATCHES "^#define QUICKOPEN_VERSION_STR \"(.*)\"$")
        set(headerVersion ${CMAKE_MATCH_1})
    elseif(thisLine MATCHES "^#define QUICKOPEN_SHORT_DESC \"(.*)\"$")
        set(headerShortDesc ${CMAKE_MATCH_1})
    elseif(thisLine MATCHES "^#define QUICKOPEN_DEVELOPER \"(.*)\"$")
        set(CPACK_PACKAGE_CONTACT ${CMAKE_MATCH_1})
    elseif(thisLine MATCHES "^#define QUICKOPEN_VENDOR \"(.*)\"$")
        set(CPACK_PACKAGE_VENDOR ${CMAKE_MATCH_1})
    elseif(thisLine MATCHES "^#define QUICKOPEN_WINDOWS_UPGRADE_GUID \"(.*)\"$")
        set(upgradeUUID ${CMAKE_MATCH_1})
    endif()
endforeach()

if(DEFINED headerVersion)
    project ("QuickOpen" VERSION ${headerVersion})
    message(STATUS "Set version to ${headerVersion}.")
else()
    project ("QuickOpen")
    message(WARNING "Could not identify project version from header file.")
endif()

if(DEFINED headerShortDesc)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${headerShortDesc}")
else()
    message(WARNING "Could not find package summary in header file.")
endif()

if(DEFINED upgradeUUID)
    set(CPACK_WIX_UPGRADE_GUID ${upgradeUUID})
    string(UUID thisBuildUUID NAMESPACE "${upgradeUUID}" NAME "${CMAKE_PROJECT_VERSION}-${CMAKE_BUILD_TYPE}" TYPE SHA1)
    set(CPACK_WIX_PRODUCT_GUID ${thisBuildUUID})
endif()

# Include sub-projects.
add_subdirectory("QuickOpen")
add_subdirectory("Tests")
add_subdirectory("NautilusExt")

include(InstallRequiredSystemLibraries)
include(CPack)

message(WARNING "GUID: ${CPACK_WIX_PRODUCT_GUID}")