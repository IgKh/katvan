set(SOURCES
    macshell_appdelegate.mm
    macshell_colorconverter.mm
    macshell_editorstatusbar.mm
    macshell_editorview.mm
    macshell_exporter.mm
    macshell_historicwindowcontroller.mm
    macshell_issuelist.mm
    macshell_labelsview.mm
    macshell_mainmenu.mm
    macshell_outlineview.mm
    macshell_previewer.mm
    macshell_settingsdialog.mm
    macshell_settingsmanager.mm
    macshell_sidebar.mm
    macshell_spellchecker.mm
    macshell_textfinderclient.mm
    macshell_typstdocument.mm
    macshell_widgets.mm
    macshell_windowcontroller.mm
    main.mm
)

qt_add_resources(ASSET_SOURCES ${PROJECT_SOURCE_DIR}/assets/assets.qrc)

set(macos_icon "${PROJECT_SOURCE_DIR}/assets/katvan.icns")
set_source_files_properties(${macos_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
list(APPEND ASSET_SOURCES ${macos_icon})

add_executable(macshell ${SOURCES} ${ASSET_SOURCES})
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

# Add core translations as Qt resource
foreach(QM_FILE IN LISTS KATVAN_CORE_QM_FILES)
    get_filename_component(QM_NAME ${QM_FILE} NAME)
    set_source_files_properties(${QM_FILE} PROPERTIES QT_RESOURCE_ALIAS ${QM_NAME})
endforeach()

qt_add_resources(macshell katvan_translations
    PREFIX "i18n"
    FILES ${KATVAN_CORE_QM_FILES}
)

# Collect Apple string files for macshell strings
set_source_files_properties(translations/en.lproj/Localizable.stringsdict PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/en.lproj")
target_sources(macshell PRIVATE translations/en.lproj/Localizable.stringsdict)

foreach(LANG IN LISTS TRANSLATION_LANGUAGES)
    set(LANG_STRINGS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/translations/${LANG}.lproj/Localizable.strings")
    set(LANG_STRINGSDICT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/translations/${LANG}.lproj/Localizable.stringsdict")
    if(NOT EXISTS "${LANG_STRINGS_FILE}")
        file(TOUCH "${LANG_STRINGS_FILE}/")
    endif()

    set_source_files_properties(${LANG_STRINGS_FILE} ${LANG_STRINGSDICT_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${LANG}.lproj")
    target_sources(macshell PRIVATE ${LANG_STRINGS_FILE} ${LANG_STRINGSDICT_FILE})
endforeach()

# Update Apple string files as needed based on the code
find_program(BARTYCROUCH NAMES bartycrouch)
if (BARTYCROUCH)
    add_custom_command(
        OUTPUT bcstrings.stamp
        DEPENDS ${SOURCES}
        COMMAND ${BARTYCROUCH} update -v
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_custom_target(bcstrings ALL DEPENDS bcstrings.stamp)
endif()
