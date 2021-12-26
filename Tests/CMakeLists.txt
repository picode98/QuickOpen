find_package(Catch2 REQUIRED)

include("../QuickOpenBuildSettings.cmake")


add_executable(test_driver TestMain.cpp UtilsTests.cpp WebServerTests.cpp
"../QuickOpen/AppConfig.cpp" "../QuickOpen/AppGUI.cpp" "../QuickOpen/GUIUtils.cpp" "../QuickOpen/TrayStatusWindow.cpp" "../QuickOpen/Utils.cpp" "../QuickOpen/WebServer.cpp" "../QuickOpen/PlatformUtils.cpp")
target_include_directories(test_driver PRIVATE "../QuickOpen")
set_property(TARGET test_driver PROPERTY COMPILE_DEFINITIONS "MOCK_CIVETWEB=1")

apply_QuickOpen_build_settings(test_driver)

target_link_libraries(test_driver PRIVATE Catch2::Catch2)

install(TARGETS test_driver RUNTIME DESTINATION bin)

include(CTest)
include(Catch)
catch_discover_tests(test_driver)