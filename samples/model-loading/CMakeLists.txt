
set(MODEL_LOADING_SAMPLE_SRC src/main.cpp)

add_executable(kisha-sample-model-loading ${MODEL_LOADING_SAMPLE_SRC})
add_executable(kisha::sample::model-loading ALIAS kisha-sample-model-loading)
add_compile_options(kisha-sample-model-loading PUBLIC cxx_std_23)
setup_common_compile_options(kisha-sample-model-loading)

target_include_directories(kisha-sample-model-loading PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kisha-sample-model-loading kisha_engine fastgltf)