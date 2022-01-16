find_package(gRPC REQUIRED)

# Proto file
set(fam_proto "fam.proto")

# Generated sources
set(fam_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/fam.pb.cc")
set(fam_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/fam.pb.h")
set(fam_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/fam.grpc.pb.cc")
set(fam_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/fam.grpc.pb.h")
add_custom_command(
      OUTPUT "${fam_proto_srcs}" "${fam_proto_hdrs}" "${fam_grpc_srcs}" "${fam_grpc_hdrs}"
      COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
      --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
      -I "${CMAKE_CURRENT_SOURCE_DIR}"
        --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_PROGRAM}"
        "${fam_proto}"
      DEPENDS "${fam_proto}")

add_library(fam_grpc_proto
  ${fam_grpc_srcs}
  ${fam_grpc_hdrs}
  ${fam_proto_srcs}
  ${fam_proto_hdrs})

target_link_libraries(fam_grpc_proto
  PUBLIC
  gRPC::grpc++_reflection
  gRPC::grpc++
  protobuf::libprotobuf
  )

target_include_directories(fam_grpc_proto
  PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR})
