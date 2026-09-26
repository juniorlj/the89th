CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v3.15.3
    GIT_SHALLOW YES
    EXCLUDE_FROM_ALL YES
)

add_executable(the89th_tests
    test_traversal.cpp
    test_transparency.cpp
    test_pitch.cpp
    test_splice.cpp
    test_bandwidth.cpp
    test_converter.cpp
    test_xing.cpp
    test_filters.cpp
    test_machine.cpp
    test_glide.cpp
    test_modern.cpp
    test_keys.cpp
)

target_link_libraries(the89th_tests PRIVATE
    the89th::core
    Catch2::Catch2WithMain
)

target_include_directories(the89th_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(the89th_tests)
