find_package(PkgConfig)
pkg_check_modules(onnxruntime REQUIRED libonnxruntime)

find_library(
    onnxruntime_FOUND_LIBRARY
    NAMES onnxruntime
    PATH_SUFFIXES lib
    HINTS ${onnxruntime_LIBRARY_DIRS}
)

find_path(
    onnxruntime_FOUND_INCLUDE_DIR onnxruntime/core/session/onnxruntime_cxx_api.h
    PATH_SUFFIXES include
    HINTS ${onnxruntime_INCLUDE_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(onnxruntime REQUIRED_VARS onnxruntime_FOUND_INCLUDE_DIR onnxruntime_FOUND_LIBRARY)

if (onnxruntime_FOUND)
    set(onnxruntime_INCLUDE_DIRS ${onnxruntime_FOUND_INCLUDE_DIR})
    set(onnxruntime_LIBRARIES ${onnxruntime_FOUND_LIBRARY})
    if (NOT TARGET onnxruntime::onnxruntime)
        add_library(onnxruntime::onnxruntime UNKNOWN IMPORTED)
    endif()
    set_target_properties(onnxruntime::onnxruntime
        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
        ${onnxruntime_INCLUDE_DIRS}
    )
    set_target_properties(onnxruntime::onnxruntime
        PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION ${onnxruntime_LIBRARIES}
    )
    mark_as_advanced(onnxruntime_INCLUDE_DIRS onnxruntime_LIBRARIES)
endif()

