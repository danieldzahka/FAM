add_subdirectory(rpc)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_library(FAM client.cpp server.cpp FAM_rdma.cpp util.cpp)
target_link_libraries(
  FAM
  PRIVATE project_options
          project_warnings
          fmt::fmt
          spdlog::spdlog
          fam_grpc_proto
          rdmacm
          ibverbs
          )
target_include_directories(FAM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
