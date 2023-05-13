macro(apply_QuickOpen_build_settings target_name)
    find_package(wxWidgets REQUIRED core base)
    include(${wxWidgets_USE_FILE})
    target_include_directories(${target_name} PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_link_libraries(${target_name} PRIVATE ${wxWidgets_LIBRARIES})

    find_package(nlohmann_json CONFIG REQUIRED)
    target_link_libraries(${target_name} PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

    find_package(civetweb REQUIRED)
    target_link_libraries(${target_name} PRIVATE civetweb::civetweb civetweb::civetweb-cpp)

    if(MSVC)
        target_link_libraries(${target_name} PRIVATE "bcrypt.lib" "wbemuuid.lib" "dbghelp.lib")
        set_property(TARGET ${target_name} PROPERTY VS_DPI_AWARE ON)
    endif()

    set_property(TARGET ${target_name} PROPERTY CXX_STANDARD 17)
endmacro()