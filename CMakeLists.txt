cmake_minimum_required(VERSION 3.22)
project(TruePrompter)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static" FORCE)

set(Protobuf_USE_STATIC_LIBS ON)
set(OPENSSL_USE_STATIC_LIBS ON)
set(onnxruntime_BUILD_SHARED_LIB ON)
set(onnxruntime_BUILD_UNIT_TESTS OFF)

set(PA_USE_ALSA ON CACHE BOOL "" FORCE)
# Do not disable logging in compile time
add_definitions(-DSPDLOG_ACTIVE_LEVEL=0)
# Do not use implicit conversions for json
add_definitions(-DJSON_USE_IMPLICIT_CONVERSIONS=0)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(onnxruntime REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(websocketpp REQUIRED)

include(cmake/proto_utils.cmake)

# Some magic for matplotplusplus to work
add_library(std_filesystem trueprompter/recognition/dummy.cpp)
add_library(std::filesystem ALIAS std_filesystem)
add_subdirectory(deps/matplotplusplus EXCLUDE_FROM_ALL)
set_property(TARGET matplot PROPERTY CXX_STANDARD 17)

add_subdirectory(deps/argumentum EXCLUDE_FROM_ALL)
add_subdirectory(deps/avcpp EXCLUDE_FROM_ALL)
add_subdirectory(deps/json EXCLUDE_FROM_ALL)
add_subdirectory(deps/libsamplerate EXCLUDE_FROM_ALL)
add_subdirectory(deps/openfst EXCLUDE_FROM_ALL)
add_subdirectory(deps/portaudio EXCLUDE_FROM_ALL)
add_subdirectory(deps/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(deps/utfcpp EXCLUDE_FROM_ALL)

add_subdirectory(cmake/phonetisaurus EXCLUDE_FROM_ALL)

add_library(websocketpp_complete INTERFACE)
target_link_libraries(websocketpp_complete INTERFACE websocketpp::websocketpp OpenSSL::SSL Boost::headers) # OpenSSL::Crypto

add_subdirectory(trueprompter)

