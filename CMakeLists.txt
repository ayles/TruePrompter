cmake_minimum_required(VERSION 3.22)
project(TruePrompter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(KALDI_BUILD_EXE OFF CACHE BOOL "Disable Kaldi exe")
set(KALDI_BUILD_TESTS OFF CACHE BOOL "Disable Kaldi tests")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static")

set(Boost_USE_STATIC_LIBS ON)
set(Protobuf_USE_STATIC_LIBS ON)
set(OPENSSL_USE_STATIC_LIBS ON)

# Do not disable logging in compile time
add_definitions(-DSPDLOG_ACTIVE_LEVEL=0)

find_package(Boost REQUIRED COMPONENTS system)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(websocketpp REQUIRED)
find_package(Protobuf REQUIRED)

include(cmake/proto_utils.cmake)

add_subdirectory(deps/kaldi)
add_subdirectory(deps/portaudio)
add_subdirectory(deps/utfcpp)
add_subdirectory(deps/spdlog)
add_subdirectory(deps/avcpp)
add_subdirectory(cmake/phonetisaurus)

add_subdirectory(trueprompter)
