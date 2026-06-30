
include(CompileShaders)

add_shader(basic INPUT shaders/fragment.slang shaders/vertex.slang OUTPUT output.spv)

add_shader(triangle_vert INPUT shaders/triangle.vert.slang OUTPUT triangle.vert.spv)
add_shader(triangle_frag INPUT shaders/triangle.frag.slang OUTPUT triangle.frag.spv)

set(KISHA_TRIANGLE_VERT_SPV "${CMAKE_CURRENT_BINARY_DIR}/triangle.vert.spv"
    CACHE INTERNAL "Path to the compiled triangle vertex SPIR-V")
set(KISHA_TRIANGLE_FRAG_SPV "${CMAKE_CURRENT_BINARY_DIR}/triangle.frag.spv"
    CACHE INTERNAL "Path to the compiled triangle fragment SPIR-V")