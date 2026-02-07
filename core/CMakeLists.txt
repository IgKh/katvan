find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.9.0)
    find_package(Qt6 COMPONENTS GuiPrivate)
endif()

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
    katvan_aboutdialog.cpp
    katvan_codemodel.cpp
    katvan_completionmanager.cpp
    katvan_coreutils.cpp
    katvan_diagnosticsmodel.cpp
    katvan_document.cpp
    katvan_editor.cpp
    katvan_editorlayout.cpp
    katvan_editorsettings.cpp
    katvan_editortheme.cpp
    katvan_editortooltip.cpp
    katvan_highlighter.cpp
    katvan_outlinemodel.cpp
    katvan_parsing.cpp
    katvan_previewerview.cpp
    katvan_spellchecker.cpp
    katvan_symbolpicker.cpp
    katvan_text_utils.cpp
    katvan_typstdriverwrapper.cpp
    katvan_wordcounter.cpp
)

qt_add_resources(SOURCES themes/themes.qrc)

if(APPLE)
    list(APPEND SOURCES
        katvan_spellchecker_macos.mm
    )
elseif(WIN32)
    list(APPEND SOURCES
        katvan_spellchecker_windows.cpp
    )
else()
    list(APPEND SOURCES
        katvan_spellchecker_hunspell.cpp
    )
endif()

add_library(katvan_core STATIC ${SOURCES})

target_include_directories(katvan_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(katvan_core PUBLIC QT_RESTRICTED_CAST_FROM_ASCII QT_NO_EMIT)

if(MSVC)
    target_compile_options(katvan_core PUBLIC /W4)
else()
    target_compile_options(katvan_core PUBLIC -Wall -Wextra -pedantic -Wno-switch)
endif()

target_link_libraries(katvan_core PUBLIC
    typstdriver
    Qt6::Widgets
)

target_link_libraries(katvan_core PRIVATE
    Qt6::GuiPrivate
)

if(NOT APPLE AND NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(hunspell REQUIRED IMPORTED_TARGET hunspell)

    target_link_libraries(katvan_core PUBLIC PkgConfig::hunspell)
endif()

qt_add_translations(katvan_core
    SOURCE_TARGETS katvan_core
    TS_FILE_DIR translations
    TS_FILE_BASE core
    QM_FILES_OUTPUT_VARIABLE KATVAN_CORE_QM_FILES
    LUPDATE_OPTIONS -no-obsolete -locations none
)

# Publish variable to shells
set(KATVAN_CORE_QM_FILES "${KATVAN_CORE_QM_FILES}" PARENT_SCOPE)
