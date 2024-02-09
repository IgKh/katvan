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
    katvan_highlighter.cpp
    katvan_mainwindow.cpp
    katvan_parsing.cpp
    katvan_recentfiles.cpp
    katvan_searchbar.cpp
    katvan_typstdriver.cpp
)

set(MAIN_SOURCES
    main.cpp
)

qt_add_resources(MAIN_SOURCES ${PROJECT_SOURCE_DIR}/assets/assets.qrc)
if(WIN32)
    list(APPEND MAIN_SOURCES ${PROJECT_SOURCE_DIR}/assets/katvan.rc)
endif()

add_library(libkatvan STATIC ${SOURCES})

target_include_directories(libkatvan PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(libkatvan PUBLIC QT_RESTRICTED_CAST_FROM_ASCII)

target_link_libraries(libkatvan PUBLIC
    Qt6::Widgets
    Qt6::PdfWidgets
)

add_executable(katvan ${MAIN_SOURCES})
target_link_libraries(katvan PRIVATE libkatvan)

if(KATVAN_PORTABLE_BUILD)
    target_compile_definitions(katvan PRIVATE KATVAN_PORTABLE_BUILD)
endif()

set_target_properties(katvan PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

qt_add_translations(katvan
    TS_FILES
        translations/katvan_he.ts
    SOURCES
        ${SOURCES} ${MAIN_SOURCES}
)
