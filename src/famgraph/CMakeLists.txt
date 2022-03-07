add_subdirectory(fgidx)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(TBB REQUIRED)

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
)
target_include_directories(famgraph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
