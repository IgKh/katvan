cmake_minimum_required(VERSION 3.19)
project(katvan LANGUAGES CXX VERSION "0.9.0")

enable_testing()

#
# The Rust standard library for MSVC targets always requests linking to the non
# debug CRT library, no matter the profile. However actual linking of the
# typstdriver DLL is done by the C++ compiler, which might try to use the debug
# CRT. Also we could work with pre-built Qt that uses the debug CRT. This makes
# things... not work. Until something like the -Zlink-directives flag for rustc
# stabilizes, Windows builds MUST NOT be done in Debug mode.
#
# See: https://github.com/rust-lang/rust/issues/39016
#
if(MSVC)
    list(REMOVE_ITEM CMAKE_CONFIGURATION_TYPES "Debug")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(typstdriver)
add_subdirectory(core)
add_subdirectory(shell)
add_subdirectory(tests)

if(APPLE)
    set_property(TARGET katvan PROPERTY INSTALL_RPATH "@executable_path/../Frameworks/")
    install(TARGETS katvan DESTINATION ${CMAKE_BINARY_DIR}/dist)
    install(TARGETS typstdriver DESTINATION ${CMAKE_BINARY_DIR}/dist/Katvan.app/Contents/Frameworks)
elseif(UNIX)
    include(GNUInstallDirs)

    if(APPIMAGE_INSTALL)
        set_property(TARGET katvan PROPERTY INSTALL_RPATH "$ORIGIN/../lib")
        install(TARGETS typstdriver DESTINATION lib)
    else()
        set_property(TARGET katvan PROPERTY INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/katvan:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
        install(TARGETS typstdriver DESTINATION ${CMAKE_INSTALL_LIBDIR}/katvan)
    endif()

    install(TARGETS katvan DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/katvan.desktop DESTINATION share/applications)
    install(FILES assets/katvan.svg DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES assets/katvan_48x48.png DESTINATION share/icons/hicolor/48x48/apps RENAME katvan.png)
endif()
