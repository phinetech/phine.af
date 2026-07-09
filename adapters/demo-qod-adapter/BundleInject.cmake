if(CMAKE_VERSION VERSION_LESS 3.19)
    message(FATAL_ERROR
        "Demo QoD bundle injection requires CMake 3.19 or newer for cmake_language(DEFER).")
endif()

if(NOT BUILD_BUNDLED)
    message(FATAL_ERROR
        "BundleInject.cmake must be used with -DBUILD_BUNDLED=ON.")
endif()

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_BINARY_DIR}/demo-qod-adapter")