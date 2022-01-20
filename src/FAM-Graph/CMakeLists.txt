add_subdirectory(fgidx)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_library(famgraph famgraph.cpp)
target_link_libraries(
  famgraph
  PRIVATE project_options
          project_warnings
          fmt::fmt
          spdlog::spdlog
          FAM
          fgidx
          )
target_include_directories(famgraph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
