find_package(Qt6 REQUIRED COMPONENTS Widgets PdfWidgets)

qt_standard_project_setup()

add_executable(katvan
    katvan_editor.cpp
    katvan_mainwindow.cpp
    katvan_typstdriver.cpp
    main.cpp
)

target_compile_definitions(katvan PRIVATE QT_RESTRICTED_CAST_FROM_ASCII)

target_link_libraries(katvan PRIVATE
    Qt6::Widgets
    Qt6::PdfWidgets
)

set_target_properties(katvan PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
