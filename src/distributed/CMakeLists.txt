find_package(MPI REQUIRED)
add_library(famgraph_distributed INTERFACE)
target_link_libraries(famgraph_distributed
        INTERFACE
        project_options
        project_warnings
        famgraph
        MPI::MPI_CXX
        )
target_include_directories(famgraph_distributed
        INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)