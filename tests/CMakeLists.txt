find_package(GTest)
if(NOT GTest_FOUND)
    message(WARNING "GoogleTest not found, skipping tests")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(katvan_tests
    katvan_codemodel.t.cpp
    katvan_editor.t.cpp
    katvan_editorsettings.t.cpp
    katvan_parsing.t.cpp
    katvan_testutils.cpp
    main.cpp
)

if(NOT APPLE AND NOT WIN32)
    target_sources(katvan_tests PRIVATE katvan_spellchecker.t.cpp)
endif()

target_include_directories(katvan_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Link with gmock only due to https://github.com/google/googletest/issues/2157#issuecomment-674361850
target_link_libraries(katvan_tests PRIVATE
    Qt6::Test
    GTest::gmock
    katvan_core
)

add_custom_target(test_dicts ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/hunspell $<TARGET_FILE_DIR:katvan_tests>/hunspell
    COMMENT "Copying mock dictionaries"
)

include(GoogleTest)
gtest_discover_tests(katvan_tests)
