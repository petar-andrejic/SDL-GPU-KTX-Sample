cmake_minimum_required(VERSION 3.26)
project(SDL3_GPU_KTX_Demo)

set(CMAKE_C_STANDARD 17)

include(cmake/compile_shaders.cmake)

find_package(SDL3 REQUIRED)
find_package(ktx REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(app
    app_state.c
    app_state.h
    error_check.c
    error_check.h
    ktx_helpers.c
    ktx_helpers.h
    main.c
    upload.c
    upload.h
)

target_link_libraries(app PUBLIC
    SDL3::SDL3
    ktx
    ${Vulkan_LIBRARIES}
)

target_compile_definitions(app PRIVATE
    $<IF:$<CONFIG:Debug>,APP_DEBUG_MODE=1,APP_DEBUG_MODE=0>
)

add_custom_target(copy_assets
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
        "${CMAKE_CURRENT_BINARY_DIR}/assets/"
    COMMENT "Copying assets"
)

add_dependencies(app copy_assets)

add_subdirectory(shaders)



