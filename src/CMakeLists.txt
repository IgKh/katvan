find_package(Qt6 REQUIRED COMPONENTS Widgets PdfWidgets LinguistTools)

qt_standard_project_setup()

find_package(Git)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE KATVAN_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(KATVAN_GIT_SHA "unknown")
endif()
configure_file(katvan_version.h.in katvan_version.h @ONLY)

set(SOURCES
    katvan_editor.cpp
    katvan_mainwindow.cpp
    katvan_recentfiles.cpp
    katvan_searchbar.cpp
    katvan_typstdriver.cpp
    main.cpp
)

qt_add_resources(SOURCES ${PROJECT_SOURCE_DIR}/assets/assets.qrc)

add_executable(katvan ${SOURCES})

target_include_directories(katvan PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(katvan PRIVATE QT_RESTRICTED_CAST_FROM_ASCII)

target_link_libraries(katvan PRIVATE
    Qt6::Widgets
    Qt6::PdfWidgets
)

set_target_properties(katvan PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

qt_add_translations(katvan TS_FILES
    translations/katvan_he.ts
)
