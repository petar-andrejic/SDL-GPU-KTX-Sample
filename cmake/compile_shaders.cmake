function(add_shaders TARGET_NAME)
    set(SHADERS ${ARGN})
    message("VERBOSE" "Creating shader target ${TARGET_NAME} with sources ${SHADERS}")
    set (SHADERS_IN_DIR ${CMAKE_CURRENT_LIST_DIR})
    set (SHADERS_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    set (SLANG_ARGS -target spirv -profile spirv_1_3 -emit-spirv-directly -fvk-use-c-layout )
    # file(GLOB SHADERS "${SHADERS_IN_DIR}/*.slang")
    add_custom_command (
        OUTPUT ${SHADERS_OUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_OUT_DIR}
    )
    foreach(SHADER ${SHADERS})
        cmake_path(GET SHADER STEM SHADER_NAME)
        # cmake_path(GET SHADER PARENT_PATH SHADERS_IN_DIR)
        set(SHADER_OUT_NAME "${SHADERS_OUT_DIR}/${SHADER_NAME}.spv")
        message("VERBOSE" "Working directory is ${SHADERS_IN_DIR}")
        list(APPEND SHADER_OUT_NAMES ${SHADER_OUT_NAME})
        add_custom_command (
            MAIN_DEPENDENCY ${SHADER}
            OUTPUT  ${SHADER_OUT_NAME}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER} ${SLANG_ARGS} -o ${SHADER_OUT_NAME}
            WORKING_DIRECTORY ${SHADERS_IN_DIR}
            COMMENT "Compiling Slang Shaders"
            VERBATIM
        )
    endforeach()
    add_custom_target (${TARGET_NAME} DEPENDS ${SHADER_OUT_NAMES})
endfunction()