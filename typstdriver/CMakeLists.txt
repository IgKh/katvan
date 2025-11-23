find_package(Qt6 REQUIRED COMPONENTS Gui Network)

if(APPLE AND NOT LibArchive_INCLUDE_DIR)
    find_program(HOMEBREW_EXECUTABLE brew)
    if(HOMEBREW_EXECUTABLE)
        execute_process(
            COMMAND ${HOMEBREW_EXECUTABLE} --prefix libarchive
            OUTPUT_VARIABLE _LibArchive_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(_LibArchive_PREFIX)
            message(STATUS "LibArchive found as Homebrew keg at ${_LibArchive_PREFIX}")
            set(LibArchive_INCLUDE_DIR "${_LibArchive_PREFIX}/include" CACHE PATH "Location of libarchive headers")
        endif()
    endif()
endif()
find_package(LibArchive REQUIRED)

find_package(Corrosion 0.6.0 QUIET)
if(NOT Corrosion_FOUND)
    include(FetchContent)

    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.6.0
    )

    FetchContent_MakeAvailable(Corrosion)
endif()

corrosion_import_crate(
    MANIFEST_PATH rust/Cargo.toml
    PROFILE ${KATVAN_CARGO_PROFILE}
    FROZEN
)

if(KATVAN_FLATPAK_BUILD)
    corrosion_set_features(typstdriver_rs FEATURES flatpak)
endif()

corrosion_add_cxxbridge(typstdriver_ffi
    CRATE typstdriver_rs
    FILES bridge.rs
)
target_include_directories(typstdriver_ffi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET typstdriver_ffi PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(typstdriver SHARED
    typstdriver_compilersettings.cpp
    typstdriver_engine.cpp
    typstdriver_logger.cpp
    typstdriver_outlinenode.cpp
    typstdriver_packagemanager.cpp
)

include(GenerateExportHeader)
generate_export_header(typstdriver)

set_target_properties(typstdriver PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    FRAMEWORK TRUE
)

target_include_directories(typstdriver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(typstdriver PUBLIC QT_RESTRICTED_CAST_FROM_ASCII QT_NO_EMIT)

target_link_libraries(typstdriver PRIVATE typstdriver_rs typstdriver_ffi)
target_link_libraries(typstdriver PUBLIC Qt6::Gui Qt6::Network LibArchive::LibArchive)
