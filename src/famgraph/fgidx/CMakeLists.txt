find_package(spdlog REQUIRED)
find_package(Boost REQUIRED COMPONENTS boost filesystem)

add_library(fgidx fgidx.cpp)
target_link_libraries(
        fgidx
        PRIVATE project_options
        project_warnings
        spdlog::spdlog
        Boost::boost
        Boost::filesystem
)
target_include_directories(fgidx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
