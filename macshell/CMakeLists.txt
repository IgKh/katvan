set(SOURCES
    macshell_appdelegate.mm
    macshell_colorconverter.mm
    macshell_editorview.mm
    macshell_mainmenu.mm
    macshell_previewer.mm
    macshell_textfinderclient.mm
    macshell_typstdocument.mm
    macshell_windowcontroller.mm
    main.mm
)

qt_add_resources(SOURCES ${PROJECT_SOURCE_DIR}/assets/assets.qrc)

set(macos_icon "${PROJECT_SOURCE_DIR}/assets/katvan.icns")
set_source_files_properties(${macos_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
list(APPEND SOURCES ${macos_icon})

add_executable(macshell ${SOURCES})
target_link_libraries(macshell PRIVATE katvan_core)

target_compile_options(macshell PRIVATE
    -fobjc-arc
)

set_target_properties(macshell PROPERTIES
    OUTPUT_NAME Katvan
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    XCODE_EMBED_FRAMEWORKS typstdriver
)
