# Protobuf.cmake - Helper functions for Protobuf and gRPC
#
# This module provides functions for generating C++ code from protobuf and gRPC
# definitions. It supports both standard protobuf messages and gRPC services.

# Custom function to generate protobuf and gRPC code
function(generate_grpc_cpp SRCS HDRS PROTO_FILES)
  if(NOT Protobuf_FOUND)
    message(FATAL_ERROR "Protobuf is required but not found")
  endif()

  # Get absolute paths to protoc and protoc-gen-grpc
  set(PROTOC_BIN ${Protobuf_PROTOC_EXECUTABLE})
  if(gRPC_FOUND)
    set(GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
  else()
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
    if(NOT GRPC_CPP_PLUGIN)
      message(WARNING "gRPC plugin not found. Will generate protobuf code only.")
    endif()
  endif()

  set(GENERATED_SRCS)
  set(GENERATED_HDRS)
  
  foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    get_filename_component(PROTO_PATH ${PROTO_FILE} DIRECTORY)
    
    set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.pb.h")
    
    list(APPEND GENERATED_SRCS ${PROTO_SRC})
    list(APPEND GENERATED_HDRS ${PROTO_HDR})
    
    if(GRPC_CPP_PLUGIN)
      set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.cc")
      set(GRPC_HDR "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.h")
      list(APPEND GENERATED_SRCS ${GRPC_SRC})
      list(APPEND GENERATED_HDRS ${GRPC_HDR})
      
      add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
        COMMAND ${PROTOC_BIN}
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/generated
             --grpc_out=${CMAKE_CURRENT_BINARY_DIR}/generated
             --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
             -I${PROTO_PATH}
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ code from ${PROTO_FILE}"
        VERBATIM
      )
    else()
      add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${PROTOC_BIN}
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/generated
             -I${PROTO_PATH}
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ code from ${PROTO_FILE}"
        VERBATIM
      )
    endif()
  endforeach()
  
  set(${SRCS} ${GENERATED_SRCS} PARENT_SCOPE)
  set(${HDRS} ${GENERATED_HDRS} PARENT_SCOPE)
endfunction()