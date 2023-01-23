cmake_minimum_required(VERSION 3.22)
project(TruePrompter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(KALDI_BUILD_EXE OFF CACHE BOOL "Disable Kaldi exe")
set(KALDI_BUILD_TESTS OFF CACHE BOOL "Disable Kaldi tests")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static")

set(BOOST_STATIC ON)
set(Boost_USE_STATIC_LIBS ON)
set(Protobuf_USE_STATIC_LIBS ON)
set(OPENSSL_USE_STATIC_LIBS ON)
set(onnxruntime_BUILD_SHARED_LIB ON)

# Do not disable logging in compile time
add_definitions(-DSPDLOG_ACTIVE_LEVEL=0)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(Protobuf REQUIRED)
find_package(websocketpp REQUIRED)

include(cmake/proto_utils.cmake)

add_subdirectory(deps/spdlog)
add_subdirectory(deps/utfcpp)
add_subdirectory(deps/portaudio)
add_subdirectory(deps/avcpp)
add_subdirectory(deps/onnxruntime/cmake)
add_subdirectory(deps/kaldi)
add_subdirectory(cmake/phonetisaurus)

add_library(websocketpp_complete INTERFACE)
target_link_libraries(websocketpp_complete INTERFACE websocketpp::websocketpp OpenSSL::SSL OpenSSL::Crypto Boost::headers)

add_subdirectory(trueprompter)

