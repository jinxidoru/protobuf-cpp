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

# load the dependencies
FetchContent_MakeAvailable(protobuf)

# protoc plugin
add_executable(pbcpp-plugin src/main_plugin.cpp)
target_link_libraries(pbcpp-plugin protobuf::libprotoc)
include_directories(${protobuf_SOURCE_DIR}/src)


