add_shaders(shader_target shader_2d.slang shader_3d.slang shader_arr.slang)

add_dependencies(app shader_target)