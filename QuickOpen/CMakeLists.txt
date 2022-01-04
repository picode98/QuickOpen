# CMakeList.txt : CMake project for QuickOpen, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
include("../QuickOpenBuildSettings.cmake")

project ("QuickOpen")
add_compile_definitions(QUICKOPEN_WINDOWS_PRODUCT_GUID="${thisBuildUUID}")

# Add source to this project's executable.
# add_executable (QuickOpen "QuickOpen.cpp" "QuickOpen.h")
add_executable(QuickOpenExecutable WIN32 "AppConfig.cpp" "AppGUI.cpp" "GUIUtils.cpp" "TrayStatusWindow.cpp" "Utils.cpp" "WebServer.cpp" "WebServerUtils.cpp" "ManagementServer.cpp" "PlatformUtils.cpp")
set_property(TARGET QuickOpenExecutable PROPERTY OUTPUT_NAME QuickOpen)

apply_QuickOpen_build_settings(QuickOpenExecutable)

## find_package(wxWidgets REQUIRED)
#target_include_directories(QuickOpen PRIVATE C:\\Users\\saama\\Application\\vcpkg\\installed\\x64-windows\\include\\wx)
## target_link_libraries(QuickOpen PRIVATE ${wxWidgets_LIBRARIES})

#if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
#	add_custom_command(TARGET QuickOpenExecutable POST_BUILD
#		COMMAND ${CMAKE_COMMAND} -E echo Will add $<TARGET_RUNTIME_DLLS:QuickOpenExecutable> to output directory
#		COMMAND_EXPAND_LISTS)
#	add_custom_command(TARGET QuickOpenExecutable POST_BUILD
#	  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:QuickOpenExecutable> $<TARGET_FILE_DIR:QuickOpenExecutable>
#	  COMMAND_EXPAND_LISTS
#	)
#endif()

install(TARGETS QuickOpenExecutable RUNTIME DESTINATION bin)
if(MSVC)
    install(DIRECTORY "${QuickOpen_BINARY_DIR}/$<CONFIG>/" DESTINATION bin FILES_MATCHING PATTERN "*.dll")
endif()
install(FILES ./test_icon.ico DESTINATION share/QuickOpen)
install(DIRECTORY ./static DESTINATION share/QuickOpen)


#add_custom_command(TARGET QuickOpen POST_BUILD


# set(OPTIONS WIN32)

# TODO: Add tests and install targets if needed.
