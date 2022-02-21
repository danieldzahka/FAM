add_library(famgraph_algs INTERFACE)
target_include_directories(famgraph_algs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(famgraph_algs
        INTERFACE
        project_options
        project_warnings
        famgraph)