find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

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
    katvan_backuphandler.cpp
    katvan_codemodel.cpp
    katvan_compileroutput.cpp
    katvan_editor.cpp
    katvan_editorlayout.cpp
    katvan_editorsettings.cpp
    katvan_editortheme.cpp
    katvan_highlighter.cpp
    katvan_mainwindow.cpp
    katvan_parsing.cpp
    katvan_previewer.cpp
    katvan_previewerview.cpp
    katvan_recentfiles.cpp
    katvan_searchbar.cpp
    katvan_spellchecker.cpp
    katvan_typstdriverwrapper.cpp
    katvan_utils.cpp
)

set(MAIN_SOURCES
    main.cpp
)

qt_add_resources(MAIN_SOURCES themes/themes.qrc ${PROJECT_SOURCE_DIR}/assets/assets.qrc)
if(WIN32)
    file(COPY ${PROJECT_SOURCE_DIR}/assets/katvan.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(${PROJECT_SOURCE_DIR}/assets/katvan.rc.in katvan.rc @ONLY)
    list(APPEND MAIN_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/katvan.rc)
elseif(APPLE)
    list(APPEND SOURCES
        katvan_spellchecker_macos.mm
        katvan_utils_macos.mm
    )

    set(macos_icon "${PROJECT_SOURCE_DIR}/assets/katvan.icns")
    set_source_files_properties(${macos_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND MAIN_SOURCES ${macos_icon})
endif()

add_library(libkatvan STATIC ${SOURCES})

target_include_directories(libkatvan PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(libkatvan PUBLIC QT_RESTRICTED_CAST_FROM_ASCII QT_NO_EMIT)

if(MSVC)
    target_compile_options(libkatvan PUBLIC /W4)
else()
    target_compile_options(libkatvan PUBLIC -Wall -Wextra -pedantic -Wno-switch)
endif()

target_link_libraries(libkatvan PUBLIC
    typstdriver
    Qt6::Widgets
)

if(NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(hunspell REQUIRED IMPORTED_TARGET hunspell)

    target_sources(libkatvan PRIVATE katvan_spellchecker_hunspell.cpp)
    target_link_libraries(libkatvan PUBLIC PkgConfig::hunspell)

    list(APPEND SOURCES katvan_spellchecker_hunspell.cpp)
endif()

add_executable(katvan ${MAIN_SOURCES})
target_link_libraries(katvan PRIVATE libkatvan)

if(KATVAN_DISABLE_PORTABLE)
    target_compile_definitions(katvan PRIVATE KATVAN_DISABLE_PORTABLE)
elseif(KATVAN_PORTABLE_BUILD)
    target_compile_definitions(katvan PRIVATE KATVAN_PORTABLE_BUILD)
endif()

set_target_properties(katvan PROPERTIES
    OUTPUT_NAME $<IF:$<PLATFORM_ID:Darwin>,Katvan,katvan>
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/assets/Info.plist.in
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    XCODE_EMBED_FRAMEWORKS typstdriver
)

qt_create_translation(QM_FILES ${SOURCES} ${MAIN_SOURCES} translations/katvan_en.ts OPTIONS -pluralonly -no-obsolete -locations none)
qt_create_translation(QM_FILES ${SOURCES} ${MAIN_SOURCES} translations/katvan_he.ts OPTIONS -no-obsolete -locations none)

qt_add_resources(katvan "translations"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES "${QM_FILES}"
)
