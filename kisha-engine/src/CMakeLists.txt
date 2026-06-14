add_library(kisha_engine STATIC)
add_library(kisha::engine ALIAS kisha_engine)

target_compile_features(kisha_engine PUBLIC cxx_std_23)
target_link_libraries(kisha_engine PUBLIC kisha::engine_dependencies)
target_include_directories(
        kisha_engine
        PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

# Uncomment function below to treat Warnings as errors
#setup_common_compile_options(kisha_engine)

add_subdirectory(core)
