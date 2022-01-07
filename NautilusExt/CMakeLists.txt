cmake_minimum_required (VERSION 3.8)

find_package(PkgConfig REQUIRED)
pkg_check_modules(nautExt REQUIRED libnautilus-extension)

add_library(QuickOpenNautilusExt SHARED "main.cpp")
target_include_directories(QuickOpenNautilusExt PUBLIC ${nautExt_INCLUDE_DIRS})
target_compile_options(QuickOpenNautilusExt PUBLIC ${nautExt_CFLAGS_OTHER})
target_link_libraries(QuickOpenNautilusExt ${nautExt_LIBRARIES})

install(TARGETS QuickOpenNautilusExt RUNTIME)