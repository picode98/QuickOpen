find_package(Catch2 REQUIRED)
add_executable(test_driver test_main.cpp)
target_link_libraries(test_driver PRIVATE Catch2::Catch2)

include(CTest)
include(Catch)
catch_discover_tests(test_driver)