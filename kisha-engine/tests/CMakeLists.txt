add_executable(kisha_engine_tests engine_runtime.tests.cpp engine_init_helpers.tests.cpp)
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

add_test(NAME kisha_engine_tests COMMAND kisha_engine_tests)
