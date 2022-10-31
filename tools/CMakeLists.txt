find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)
#find_package(TBB REQUIRED)
find_package(oneDPL REQUIRED)

add_executable(edgelist2fg edgelist2fg.cpp)
target_link_libraries(edgelist2fg PRIVATE
        project_options
        project_warnings
        Boost::program_options
        oneDPL
        spdlog::spdlog
        )

add_executable(fg2compressed fg2compressed.cpp)
target_link_libraries(fg2compressed PRIVATE
        project_options
        project_warnings
        Boost::program_options
        oneDPL
        spdlog::spdlog
        )