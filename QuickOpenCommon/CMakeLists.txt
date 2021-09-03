# CMakeList.txt : CMake project for QuickOpenCommon, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("QuickOpenCommon")

# Add source to this project's executable.
# add_executable (QuickOpenCommon "QuickOpenCommon.cpp" "QuickOpenCommon.h")
add_executable(QuickOpenCommon WIN32 "AppConfig.cpp" "AppGUI.cpp" "GUIUtils.cpp" "TrayStatusWindow.cpp" "Utils.cpp" "WebServer.cpp" "PlatformUtils.cpp")

## find_package(wxWidgets REQUIRED)
#target_include_directories(QuickOpenCommon PRIVATE C:\\Users\\saama\\Application\\vcpkg\\installed\\x64-windows\\include\\wx)
## target_link_libraries(QuickOpenCommon PRIVATE ${wxWidgets_LIBRARIES})

find_package(wxWidgets REQUIRED core base)
include(${wxWidgets_USE_FILE})
target_include_directories(QuickOpenCommon PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_link_libraries(QuickOpenCommon PRIVATE ${wxWidgets_LIBRARIES})

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(QuickOpenCommon PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

find_package(civetweb REQUIRED)
target_link_libraries(QuickOpenCommon PRIVATE civetweb::civetweb civetweb::civetweb-cpp)

target_link_libraries(QuickOpenCommon PRIVATE "bcrypt.lib")

set_property(TARGET QuickOpenCommon PROPERTY CXX_STANDARD 17)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
	add_custom_command(TARGET QuickOpenCommon POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E echo Will add $<TARGET_RUNTIME_DLLS:QuickOpenCommon> to output directory
		COMMAND_EXPAND_LISTS)
	add_custom_command(TARGET QuickOpenCommon POST_BUILD
	  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:QuickOpenCommon> $<TARGET_FILE_DIR:QuickOpenCommon>
	  COMMAND_EXPAND_LISTS
	)
endif()
if(MSVC)
	list(APPEND vcpkg_other_libs ${Z_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin/wxmsw314ud_core_vc_custom.dll
		${Z_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin/wxbase314ud_vc_custom.dll
		${Z_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin/jpeg62.dll
		${Z_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin/zlibd1.dll
		${Z_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin/libpng16d.dll
		${Z_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin/tiffd.dll
		${Z_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin/lzmad.dll)
	add_custom_command(TARGET QuickOpenCommon POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${vcpkg_other_libs} $<TARGET_FILE_DIR:QuickOpenCommon>
		COMMAND_EXPAND_LISTS
	)
endif()

#add_custom_command(TARGET QuickOpenCommon POST_BUILD


# set(OPTIONS WIN32)

# TODO: Add tests and install targets if needed.
