add_executable(the89th_render src/main.cpp)

target_link_libraries(the89th_render PRIVATE the89th::core)

# dr_wav is a single public-domain header, so the renderer links the core alone
# and builds in about a second. A/B iteration should not wait on a framework.
target_include_directories(the89th_render PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

set_target_properties(the89th_render PROPERTIES OUTPUT_NAME the89th-render)
