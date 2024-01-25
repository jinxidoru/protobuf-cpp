cmake_minimum_required(VERSION 3.5...3.28)
project(protobuf-cpp
  LANGUAGES CXX)

# setup compiler
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent dependencies
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_SHALLOW    TRUE
  GIT_TAG        v21.4)
set(protobuf_BUILD_TESTS OFF CACHE INTERNAL "")

FetchContent_Declare(Catch2
  GIT_SHALLOW    TRUE
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.0-preview3)

# load the dependencies
FetchContent_MakeAvailable(protobuf Catch2)

# protoc plugin
add_executable(pbcpp-plugin src/main_plugin.cpp)
target_link_libraries(pbcpp-plugin protobuf::libprotoc)
include_directories(${protobuf_SOURCE_DIR}/src)

#
set(GENERATED_CODE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(PROTO_SRCS ${GENERATED_CODE_DIR}/simple.pb.cc)
set(PROTO_HDRS ${GENERATED_CODE_DIR}/simple.pb.h)
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/test/protobufs)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
  COMMAND protobuf::protoc
  ARGS --proto_path ${PROTO_DIR} simple.proto --cpp_out ${GENERATED_CODE_DIR}
  DEPENDS ${PROTO_DIR}/simple.proto)

add_custom_command(
  OUTPUT ${GENERATED_CODE_DIR}/simple.hpp
  COMMAND protobuf::protoc
  ARGS --proto_path ${PROTO_DIR} simple.proto --pbcpp_out ${GENERATED_CODE_DIR}
       --plugin=protoc-gen-pbcpp=$<TARGET_FILE:pbcpp-plugin>
  DEPENDS ${PROTO_DIR}/simple.proto pbcpp-plugin)

add_library(proto-objects STATIC ${PROTO_SRCS} ${PROTO_HDRS} ${GENERATED_CODE_DIR}/simple.hpp)
target_link_libraries(proto-objects PRIVATE protobuf::libprotobuf)
include_directories(${GENERATED_CODE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/include)

# testing system
add_executable(tests src/test/test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain proto-objects)
add_custom_target(check
  DEPENDS $<TARGET_FILE:tests>
  COMMAND $<TARGET_FILE:tests>)

