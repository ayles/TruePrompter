add_library(phonetisaurus STATIC
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/feature-reader.cc
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/LatticePruner.cc
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/M2MFstAligner.cc
    ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/lib/util.cc
)

target_include_directories(
    phonetisaurus
    PUBLIC ${CMAKE_SOURCE_DIR}/deps/openfst/src/include
    PUBLIC ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src
    PRIVATE ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/3rdparty/rnnlm
    PRIVATE ${CMAKE_SOURCE_DIR}/deps/phonetisaurus/src/include
)

target_link_libraries(phonetisaurus PUBLIC utf8cpp fst fstngram fstfar)
set_property(TARGET phonetisaurus PROPERTY CXX_STANDARD 17)

