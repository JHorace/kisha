include_guard(GLOBAL)

function(setup_common_compile_options target)

    # Set some sane compile options
    if(MSVC)
        set(CFLAGS /W4 /WX)
    else()
        set(CFLAGS -Wall -Wextra -Werror)
    endif()

    target_compile_options(${target} PRIVATE ${CFLAGS})
endfunction()