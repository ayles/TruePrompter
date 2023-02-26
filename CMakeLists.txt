cmake_minimum_required(VERSION 3.22)
project(TruePrompter)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(KALDI_BUILD_EXE OFF CACHE BOOL "Disable Kaldi exe" FORCE)
# set(KALDI_BUILD_TESTS OFF CACHE BOOL "Disable Kaldi tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static" FORCE)
set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "FLATBUFFERS_BUILD_FLATC" FORCE)

# set(BOOST_STATIC ON)
# set(Boost_USE_STATIC_LIBS ON)
set(Protobuf_USE_STATIC_LIBS ON)
set(OPENSSL_USE_STATIC_LIBS ON)
set(onnxruntime_BUILD_SHARED_LIB ON)
set(onnxruntime_BUILD_UNIT_TESTS OFF)

# Do not disable logging in compile time
add_definitions(-DSPDLOG_ACTIVE_LEVEL=0)

set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl")

# find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(onnxruntime REQUIRED)
find_package(Matplot++ REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(websocketpp REQUIRED)

include(cmake/proto_utils.cmake)

add_subdirectory(deps/spdlog)
add_subdirectory(deps/utfcpp)
add_subdirectory(deps/portaudio)
add_subdirectory(deps/avcpp)
# add_subdirectory(deps/kaldi)
# add_subdirectory(cmake/phonetisaurus)

add_library(websocketpp_complete INTERFACE)
target_compile_definitions(websocketpp::websocketpp INTERFACE ASIO_STANDALONE)
target_link_libraries(websocketpp_complete INTERFACE websocketpp::websocketpp OpenSSL::SSL) #Boost::headers  OpenSSL::Crypto

add_subdirectory(trueprompter)

