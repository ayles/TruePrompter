cmake_minimum_required(VERSION 3.22)
project(Prompter)

set(CMAKE_CXX_STANDARD 17)

# Use faster linker
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")

# Color output for ninja
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif ()

find_package(BLAS REQUIRED)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static")

add_subdirectory(deps/kaldi)
add_subdirectory(deps/portaudio)

include(cmake/phonetisaurus.cmake)

target_link_libraries(kaldi-online PUBLIC PortAudio)

add_executable(Prompter main.cpp model.cpp recognizer.cpp)
target_include_directories(Prompter PUBLIC ${openfst_SOURCE_DIR}/src/include)
target_link_libraries(Prompter PortAudio fst fstngram kaldi-fstext kaldi-nnet3 kaldi-online kaldi-online2 kaldi-rnnlm phonetisaurus)
