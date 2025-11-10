find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

set(SOURCES
    katvan_backuphandler.cpp
    katvan_compileroutput.cpp
    katvan_infobar.cpp
    katvan_labelsview.cpp
    katvan_mainwindow.cpp
    katvan_outlineview.cpp
    katvan_previewer.cpp
    katvan_recentfiles.cpp
    katvan_searchbar.cpp
    katvan_settingsdialog.cpp
    katvan_utils.cpp
    main.cpp
)

qt_add_resources(SOURCES ${PROJECT_SOURCE_DIR}/assets/assets.qrc)
qt_add_resources(SOURCES ${PROJECT_SOURCE_DIR}/assets/icons.qrc)
if(WIN32)
    file(COPY ${PROJECT_SOURCE_DIR}/assets/katvan.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(${PROJECT_SOURCE_DIR}/assets/katvan.rc.in katvan.rc @ONLY)
    list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/katvan.rc)
elseif(APPLE)
    list(APPEND SOURCES
        katvan_utils_macos.mm
    )

    set(macos_icon "${PROJECT_SOURCE_DIR}/assets/katvan.icns")
    set_source_files_properties(${macos_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND SOURCES ${macos_icon})
endif()

add_executable(katvan ${SOURCES})
target_link_libraries(katvan PRIVATE katvan_core)

if(KATVAN_DISABLE_PORTABLE OR KATVAN_FLATPAK_BUILD)
    target_compile_definitions(katvan PRIVATE KATVAN_DISABLE_PORTABLE)
elseif(KATVAN_PORTABLE_BUILD)
    target_compile_definitions(katvan PRIVATE KATVAN_PORTABLE_BUILD)
endif()

if(KATVAN_FLATPAK_BUILD)
    target_compile_definitions(katvan PRIVATE KATVAN_FLATPAK_BUILD)
endif()

set_target_properties(katvan PROPERTIES
    OUTPUT_NAME $<IF:$<PLATFORM_ID:Darwin>,Katvan,katvan>
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/assets/Info.plist.in
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    XCODE_EMBED_FRAMEWORKS typstdriver
)

qt_add_translations(katvan
    SOURCE_TARGETS
        katvan
        katvan_core
    TS_FILES
        translations/katvan_he.ts
    PLURALS_TS_FILE
        translations/katvan_en.ts
    LUPDATE_OPTIONS
        -no-obsolete -locations none
)
