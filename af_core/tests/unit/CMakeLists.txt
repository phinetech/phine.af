# Light unit-test scaffold for af_core.
#
# This intentionally covers only one real class (UeKey) to prove the
# ctest/CI wiring end-to-end. Extensive coverage is expected to follow —
# add new TEST() cases / source files here as af_core grows unit tests.

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(af_core_unit_tests
    test_ue_key.cpp
)

target_include_directories(af_core_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(af_core_unit_tests PRIVATE
    af_core_lib
    GTest::gtest_main
)

# Covers any header-only code a test might include directly rather than
# through af_core_lib (that code compiles into this binary, not the lib).
if(ENABLE_COVERAGE)
    target_compile_options(af_core_unit_tests PRIVATE --coverage -O0 -g)
    target_link_options(af_core_unit_tests PRIVATE --coverage)
endif()

include(GoogleTest)
gtest_discover_tests(af_core_unit_tests
    PROPERTIES LABELS "unit"
)
