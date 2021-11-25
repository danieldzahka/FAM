find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)

add_library(FAM client.cpp server.cpp)
target_link_libraries(
  FAM
  PRIVATE project_options
          project_warnings
          fmt::fmt
          spdlog::spdlog
          Boost::json
  PUBLIC Boost::boost)
target_include_directories(FAM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
