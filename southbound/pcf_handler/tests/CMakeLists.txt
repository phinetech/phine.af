cmake_minimum_required(VERSION 3.14)

# Prefer the system-installed GoogleTest package (apt libgtest-dev in the
# Docker tests stage); fall back to fetching it only if unavailable.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    # Prevent GoogleTest from overriding parent project compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Flow description utility tests (header-only, no heavy dependencies)
add_executable(test_flow_descriptions
    test_flow_descriptions.cpp
)

# Include paths: pcf_handler headers + common model headers
# When built via parent add_subdirectory(), COMMON_INCLUDE_DIRS is inherited.
# When built standalone, resolve common paths relative to this directory.
target_include_directories(test_flow_descriptions PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${COMMON_INCLUDE_DIRS}
)

if(NOT COMMON_INCLUDE_DIRS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../common")
    target_include_directories(test_flow_descriptions PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/models
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/models/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/component/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/communication/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/config/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/di/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/cpp_utils/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/handlers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/response/include
    )
endif()

target_link_libraries(test_flow_descriptions PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_flow_descriptions
    PROPERTIES LABELS "unit"
)
