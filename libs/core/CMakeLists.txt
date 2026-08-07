# Header-only, zero JUCE. Headless-testable and reusable on its own.
add_library(the89th_core INTERFACE)
add_library(the89th::core ALIAS the89th_core)

target_include_directories(the89th_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(the89th_core INTERFACE cxx_std_20)

# Visible in IDEs without becoming a compiled target.
file(GLOB_RECURSE THE89TH_CORE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)
target_sources(the89th_core INTERFACE FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES ${THE89TH_CORE_HEADERS})
