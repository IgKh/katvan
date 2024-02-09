enable_testing()

find_package(GTest)
if(NOT GTest_FOUND)
    message(WARNING "Googletest not found, skipping tests")
    return()
endif()

add_executable(katvan_tests
    katvan_parsing.t.cpp
)

target_include_directories(katvan_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(katvan_tests PRIVATE
    GTest::gtest_main
    GTest::gmock
    libkatvan
)

include(GoogleTest)
gtest_discover_tests(katvan_tests)
