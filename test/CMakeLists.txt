find_package(Catch2 REQUIRED)
find_package(fmt REQUIRED)

include(CTest)
include(Catch)

add_library(catch_main STATIC catch_main.cpp)
target_link_libraries(catch_main PUBLIC Catch2::Catch2)
target_link_libraries(catch_main PRIVATE project_options)

set(MEMADDR "0.0.0.0:50051" CACHE STRING "IP of Memserver Ethernet Interface")
set(MMAP_TEST1 "${CMAKE_CURRENT_SOURCE_DIR}/files/mmap_test1" CACHE STRING "test file for remote mmap")
set(FGIDX_TEST1 "${CMAKE_CURRENT_SOURCE_DIR}/files/fgidxtest1.idx" CACHE STRING "test file for .idx")
set(TEST_GRAPH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test_graphs" CACHE STRING "Graph Inputs")
set(LARGE_TEST_GRAPH_DIR "/netscratch/fam-graph" CACHE STRING "Large Graph Inputs")
configure_file("constants.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/constants.hpp")

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE project_warnings project_options catch_main FAM)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(tests server)

add_executable(fgidxtests fgidxtests.cpp)
target_link_libraries(fgidxtests PRIVATE project_warnings project_options catch_main fmt::fmt fgidx)
target_include_directories(fgidxtests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(graph_tests graph_tests.cpp)
target_link_libraries(graph_tests PRIVATE project_warnings project_options catch_main fmt::fmt famgraph)
target_include_directories(graph_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(graph_tests server)

add_executable(algorithm_tests algorithm_tests.cpp)
target_link_libraries(algorithm_tests PRIVATE project_warnings project_options catch_main fmt::fmt famgraph_algs)
target_include_directories(algorithm_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(algorithm_tests server)

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
        tests
        TEST_PREFIX
        "unittests."
        REPORTER
        xml
        OUTPUT_DIR
        .
        OUTPUT_PREFIX
        "unittests."
        OUTPUT_SUFFIX
        .xml)

catch_discover_tests(
        fgidxtests
        TEST_PREFIX
        "unittests."
        REPORTER
        xml
        OUTPUT_DIR
        .
        OUTPUT_PREFIX
        "unittests."
        OUTPUT_SUFFIX
        .xml)

catch_discover_tests(
        graph_tests
        TEST_PREFIX
        "unittests."
        REPORTER
        xml
        OUTPUT_DIR
        .
        OUTPUT_PREFIX
        "unittests."
        OUTPUT_SUFFIX
        .xml)

catch_discover_tests(
        algorithm_tests
        TEST_PREFIX
        "unittests."
        REPORTER
        xml
        OUTPUT_DIR
        .
        OUTPUT_PREFIX
        "unittests."
        OUTPUT_SUFFIX
        .xml)