add_library(phonetisaurus STATIC
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/feature-reader.cc
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/LatticePruner.cc
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/M2MFstAligner.cc
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/util.cc
)

target_include_directories(
    phonetisaurus
    PRIVATE ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/3rdparty/rnnlm
    PUBLIC ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/3rdparty/utfcpp
    PRIVATE ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/include
    PUBLIC ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src
)

target_link_libraries(phonetisaurus PRIVATE fst fstngram fstfar)
target_include_directories(phonetisaurus PRIVATE ${openfst_SOURCE_DIR}/src/include)
set_property(TARGET phonetisaurus PROPERTY CXX_STANDARD 17)
