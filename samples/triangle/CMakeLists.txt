# Windowed triangle sample executable.
#

add_executable(triangle main.cpp)
target_compile_features(triangle PRIVATE cxx_std_23)
target_link_libraries(
        triangle
        PRIVATE
        kisha::sample_framework
)

target_compile_definitions(
        triangle
        PRIVATE
        "KISHA_TRIANGLE_VERT_SPV=\"${KISHA_TRIANGLE_VERT_SPV}\""
        "KISHA_TRIANGLE_FRAG_SPV=\"${KISHA_TRIANGLE_FRAG_SPV}\""
)
add_dependencies(triangle kisha_shaders)
