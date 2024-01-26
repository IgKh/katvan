find_package(Qt6 REQUIRED COMPONENTS Core)

find_package(GTest)
if(NOT GTest_FOUND)
    message(WARNING "Googletest not found, skipping tests")
    return()
endif()

add_executable(katvan_tests
    ${PROJECT_SOURCE_DIR}/src/katvan_parsing.cpp
    katvan_parsing.t.cpp
)

target_include_directories(katvan_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(katvan_tests PRIVATE GTest::gtest_main GTest::gmock Qt6::Core)

add_test(MAME katvan_tests COMMAND katvan_tests)
