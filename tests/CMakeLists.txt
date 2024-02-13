enable_testing()

find_package(GTest)
if(NOT GTest_FOUND)
    message(WARNING "GoogleTest not found, skipping tests")
    return()
endif()

add_executable(katvan_tests
    katvan_parsing.t.cpp
    katvan_spellchecker.t.cpp
    main.cpp
)

target_include_directories(katvan_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Link with gmock only due to https://github.com/google/googletest/issues/2157#issuecomment-674361850
target_link_libraries(katvan_tests PRIVATE
    GTest::gmock
    libkatvan
)

file(COPY hunspell DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

include(GoogleTest)
gtest_discover_tests(katvan_tests)
