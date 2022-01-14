find_package(gRPC REQUIRED)

# Proto file
set(FAM_proto "FAM.proto")

# Generated sources
set(FAM_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/FAM.pb.cc")
set(FAM_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/FAM.pb.h")
set(FAM_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/FAM.grpc.pb.cc")
set(FAM_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/FAM.grpc.pb.h")
add_custom_command(
      OUTPUT "${FAM_proto_srcs}" "${FAM_proto_hdrs}" "${FAM_grpc_srcs}" "${FAM_grpc_hdrs}"
      COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
      --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
      -I "${CMAKE_CURRENT_SOURCE_DIR}"
        --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_PROGRAM}"
        "${FAM_proto}"
      DEPENDS "${FAM_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# FAM_grpc_proto
add_library(FAM_grpc_proto
  ${FAM_grpc_srcs}
  ${FAM_grpc_hdrs}
  ${FAM_proto_srcs}
  ${FAM_proto_hdrs})
target_link_libraries(FAM_grpc_proto
  PUBLIC
  gRPC::grpc++_reflection
  gRPC::grpc++
  protobuf::libprotobuf
  )

# Targets greeter_[async_](client|server)
# foreach(_target
#   greeter_client greeter_server    
#   greeter_async_server)
#   add_executable(${_target} "${_target}.cc")
#   target_link_libraries(${_target}
#     FAM_grpc_proto
# )
# endforeach()
