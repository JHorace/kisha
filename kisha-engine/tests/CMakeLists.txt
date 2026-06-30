add_executable(kisha_engine_tests engine_runtime.tests.cpp engine_init_helpers.tests.cpp rendering.tests.cpp)
target_compile_features(kisha_engine_tests PRIVATE cxx_std_23)
target_link_libraries(
        kisha_engine_tests
        PRIVATE
        kisha_engine
        kisha::engine_test_dependencies
)
target_include_directories(
        kisha_engine_tests
        PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../src/core"
)

# Hand the compiled triangle SPIR-V locations to the pipeline-creation tests, and
# make sure those shaders are built before the test executable runs.
target_compile_definitions(
        kisha_engine_tests
        PRIVATE
        "KISHA_TRIANGLE_VERT_SPV=\"${KISHA_TRIANGLE_VERT_SPV}\""
        "KISHA_TRIANGLE_FRAG_SPV=\"${KISHA_TRIANGLE_FRAG_SPV}\""
)
add_dependencies(kisha_engine_tests shader_triangle_vert shader_triangle_frag)

add_test(NAME kisha_engine_tests COMMAND kisha_engine_tests)
