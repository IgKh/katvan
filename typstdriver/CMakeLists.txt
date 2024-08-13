find_package(Qt6 REQUIRED COMPONENTS Network)

find_package(Corrosion QUIET)
if(NOT Corrosion_FOUND)
    include(FetchContent)

    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.5
    )

    FetchContent_MakeAvailable(Corrosion)
endif()

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)

corrosion_add_cxxbridge(typstdriver_ffi
    CRATE typstdriver_rs
    FILES bridge.rs
)
target_include_directories(typstdriver_ffi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET typstdriver_ffi PROPERTY POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libarchive REQUIRED IMPORTED_TARGET libarchive)

add_library(typstdriver SHARED
    typstdriver_engine.cpp
    typstdriver_logger.cpp
    typstdriver_packagemanager.cpp
)

include(GenerateExportHeader)
generate_export_header(typstdriver)

set_property(TARGET typstdriver PROPERTY CXX_VISIBILITY_PRESET hidden)

target_include_directories(typstdriver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(typstdriver PUBLIC QT_RESTRICTED_CAST_FROM_ASCII QT_NO_EMIT)

target_link_libraries(typstdriver PRIVATE typstdriver_rs typstdriver_ffi)
target_link_libraries(typstdriver PUBLIC Qt6::Network PkgConfig::libarchive)
