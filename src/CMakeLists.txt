add_subdirectory(codec)
add_subdirectory(FAM)
add_subdirectory(famgraph)
add_subdirectory(graph_algs)
if (ENABLE_DISTRIBUTED)
    add_subdirectory(distributed)
endif ()

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)

add_executable(server server.cpp)
target_link_libraries(
        server
        PRIVATE project_options
        project_warnings
        fmt::fmt
        Boost::program_options
        spdlog::spdlog
        FAM)
