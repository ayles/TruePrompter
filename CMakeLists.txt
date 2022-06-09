cmake_minimum_required(VERSION 3.22)
project(TruePrompter)

set(CMAKE_CXX_STANDARD 17)

# Use faster linker
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")

# Color output for ninja
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    add_compile_options(-fcolor-diagnostics)
endif ()

set(KALDI_BUILD_EXE OFF CACHE BOOL "Disable Kaldi exe")
set(KALDI_BUILD_TESTS OFF CACHE BOOL "Disable Kaldi tests")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static")

set(BLA_STATIC ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(websocketpp REQUIRED)
find_package(Protobuf REQUIRED)

add_subdirectory(deps/kaldi)
add_subdirectory(deps/portaudio)
add_subdirectory(deps/span)

include(cmake/phonetisaurus.cmake)

target_link_libraries(kaldi-online PUBLIC PortAudio)

add_subdirectory(proto)
add_subdirectory(src)
