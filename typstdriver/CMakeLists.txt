find_package(Qt6 REQUIRED COMPONENTS Core)

find_package(Corrosion)
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

add_library(typstdriver SHARED
    typstdriver_engine.cpp
    typstdriver_logger.cpp
)

target_include_directories(typstdriver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(typstdriver PUBLIC QT_RESTRICTED_CAST_FROM_ASCII QT_NO_EMIT)

target_link_libraries(typstdriver PRIVATE typstdriver_rs typstdriver_ffi)
target_link_libraries(typstdriver PUBLIC Qt6::Core)
