add_subdirectory(rpc)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem) #can replace with std::filesystem

set(MAX_OUTSTANDING_WR 25 CACHE STRING "Internal Buffer Size for WR's")
configure_file("FAM_constants.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/FAM_constants.hpp")

add_library(FAM client.cpp server.cpp FAM_rdma.cpp util.cpp)
target_link_libraries(
        FAM
        PRIVATE project_options
        project_warnings
        fmt::fmt
        spdlog::spdlog
        Boost::filesystem
        fam_grpc_proto
        rdmacm
        ibverbs
)

target_include_directories(FAM
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR})
