# CMakeList.txt : CMake project for QuickOpen, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("QuickOpen")
add_compile_definitions(APP_VERSION="${CMAKE_PROJECT_VERSION}")

# Add source to this project's executable.
# add_executable (QuickOpen "QuickOpen.cpp" "QuickOpen.h")
add_executable(QuickOpenExecutable WIN32 "AppConfig.cpp" "AppGUI.cpp" "GUIUtils.cpp" "TrayStatusWindow.cpp" "Utils.cpp" "WebServer.cpp" "PlatformUtils.cpp")
set_property(TARGET QuickOpenExecutable PROPERTY OUTPUT_NAME QuickOpen)

## find_package(wxWidgets REQUIRED)
#target_include_directories(QuickOpen PRIVATE C:\\Users\\saama\\Application\\vcpkg\\installed\\x64-windows\\include\\wx)
## target_link_libraries(QuickOpen PRIVATE ${wxWidgets_LIBRARIES})

find_package(wxWidgets REQUIRED core base)
include(${wxWidgets_USE_FILE})
target_include_directories(QuickOpenExecutable PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_link_libraries(QuickOpenExecutable PRIVATE ${wxWidgets_LIBRARIES})

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(QuickOpenExecutable PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

find_package(civetweb REQUIRED)
target_link_libraries(QuickOpenExecutable PRIVATE civetweb::civetweb civetweb::civetweb-cpp)

if(MSVC)
    target_link_libraries(QuickOpenExecutable PRIVATE "bcrypt.lib" "wbemuuid.lib")
    set_property(TARGET QuickOpenExecutable PROPERTY VS_DPI_AWARE ON)
endif()

set_property(TARGET QuickOpenExecutable PROPERTY CXX_STANDARD 17)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
	add_custom_command(TARGET QuickOpenExecutable POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E echo Will add $<TARGET_RUNTIME_DLLS:QuickOpenExecutable> to output directory
		COMMAND_EXPAND_LISTS)
	add_custom_command(TARGET QuickOpenExecutable POST_BUILD
	  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:QuickOpenExecutable> $<TARGET_FILE_DIR:QuickOpenExecutable>
	  COMMAND_EXPAND_LISTS
	)
endif()

#add_custom_command(TARGET QuickOpen POST_BUILD


# set(OPTIONS WIN32)

# TODO: Add tests and install targets if needed.
