include_guard(GLOBAL)

add_custom_target(kisha_shaders)

function(add_shader SHADER_NAME)
    cmake_parse_arguments(SHADER "" "OUTPUT;COMPILER_ARGS" "INPUT;ARGS" ${ARGN})

    # Make sure they added input shaders
    if(NOT SHADER_INPUT)
        message(FATAL_ERROR "add_shader: INPUT files are required")
    endif()

    # Make sure they specified an output file
    if(NOT SHADER_OUTPUT)
        message(FATAL_ERROR "add_shader: OUTPUT file is required")
    endif()

    # Make the shader recompile on being edited
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        MAIN_DEPENDENCY ${SHADER_INPUT}
        COMMAND ${SLANG_COMPILER_BIN} ${SHADER_INPUT} ${SHADER_COMPILER_ARGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_OUTPUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Compiling Shader: ${SHADER_INPUT} -> ${SHADER_OUTPUT}")

    set(SHADER_STEP_NAME "shader_${SHADER_NAME}")

    # Make it a compiler target
    add_custom_target(${SHADER_STEP_NAME}
        DEPENDS ${SHADER_OUTPUT})

    # Hook this target into the all shaders build
    add_dependencies(kisha_shaders ${SHADER_STEP_NAME})

    # Hide this shader from the default targets list in auto complete and CLion
    set_target_properties(${SHADER_STEP_NAME} PROPERTIES
        FOLDER "Shaders"
        SHOW_IN_DEFAULT_BUILD_GROUP FALSE)
endfunction()