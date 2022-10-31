find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_library(codec codec.cpp)
target_link_libraries(codec PRIVATE
        project_options
        project_warnings
        spdlog::spdlog
        fmt::fmt
        )
target_include_directories(codec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)