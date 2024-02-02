cmake_minimum_required(VERSION 3.5...3.28)
project(protobuf-cpp
  LANGUAGES CXX)

# setup compiler
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent dependencies
include(FetchContent)
FetchContent_Declare(protobuf
  URL       https://github.com/protocolbuffers/protobuf/archive/refs/tags/v21.3.tar.gz
  URL_HASH  SHA1=ee045719d0588ee47f893e27cc6181815a69d3cc)
FetchContent_Declare(Catch2
  URL       https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.tar.gz
  URL_HASH  SHA1=ce9613c9b25803a5b052fb75b71a6e14f1e95eb8)

# load the dependencies
set(protobuf_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(protobuf)

# protoc plugin
add_executable(pbcpp-plugin src/main_plugin.cpp)
target_link_libraries(pbcpp-plugin protobuf::libprotoc)
include_directories(${protobuf_SOURCE_DIR}/src)


#### TESTS
# Only execute this if we are running as the main CMake, otherwise skip it
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  FetchContent_MakeAvailable(Catch2)

  # parse all of the test protobufs
  set(GENERATED_CODE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
  set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/test/protobufs)
  make_directory(${GENERATED_CODE_DIR})
  foreach(proto simple.proto)
    get_filename_component(basename ${proto} NAME_WE)
    add_custom_command(
      OUTPUT ${GENERATED_CODE_DIR}/${basename}.pb.cc ${GENERATED_CODE_DIR}/${basename}.pb.h
      COMMAND protobuf::protoc
      ARGS --proto_path ${PROTO_DIR} ${proto} --cpp_out ${GENERATED_CODE_DIR}
           -I ${protobuf_SOURCE_DIR}/src
      DEPENDS ${PROTO_DIR}/${proto})

    add_custom_command(
      OUTPUT ${GENERATED_CODE_DIR}/${basename}.hpp
      COMMAND protobuf::protoc
      ARGS --proto_path ${PROTO_DIR} ${proto} --pbcpp_out ${GENERATED_CODE_DIR}
           --plugin=protoc-gen-pbcpp=$<TARGET_FILE:pbcpp-plugin>
           -I ${protobuf_SOURCE_DIR}/src
      DEPENDS ${PROTO_DIR}/${proto} pbcpp-plugin)
    list(APPEND proto_SRCS ${basename}.pb.cc ${basename}.pb.h ${basename}.hpp)
  endforeach()

  # setup the intermediate protobuf library
  list(TRANSFORM proto_SRCS PREPEND ${GENERATED_CODE_DIR}/)
  add_library(proto-objects STATIC ${proto_SRCS})
  target_link_libraries(proto-objects PRIVATE protobuf::libprotobuf)
  include_directories(${GENERATED_CODE_DIR})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/include)

  # testing system
  add_executable(tests src/test/test.cpp)
  target_link_libraries(tests PRIVATE Catch2::Catch2WithMain proto-objects)
  add_custom_target(check
    DEPENDS $<TARGET_FILE:tests>
    COMMAND $<TARGET_FILE:tests>)

endif()

