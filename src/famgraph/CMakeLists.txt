add_subdirectory(fgidx)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(TBB REQUIRED)
find_package(range-v3 REQUIRED)

add_library(famgraph famgraph.cpp graph.cpp algorithm.cpp)
target_link_libraries(
        famgraph
        PRIVATE project_options
        project_warnings
        fmt::fmt
        spdlog::spdlog
        PUBLIC
        TBB::tbb
        FAM
        fgidx
        range-v3::range-v3
)
target_include_directories(famgraph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
