cmake_minimum_required (VERSION 3.8)

find_package(PkgConfig)

if(${PkgConfig_FOUND})
    pkg_check_modules(nautExt libnautilus-extension)

    if(${nautExt_FOUND})
        add_library(QuickOpenNautilusExt SHARED "main.cpp")
        target_include_directories(QuickOpenNautilusExt PUBLIC ${nautExt_INCLUDE_DIRS})
        target_compile_options(QuickOpenNautilusExt PUBLIC ${nautExt_CFLAGS_OTHER})
        target_link_libraries(QuickOpenNautilusExt ${nautExt_LIBRARIES})

        install(TARGETS QuickOpenNautilusExt RUNTIME)
    else()
        message(STATUS "pkg-config did not find libnautilus-extension, so QuickOpenNautilusExt will not be built.")
    endif()
else()
    message(STATUS "pkg-config was not found, so QuickOpenNautilusExt will not be built.")
endif()