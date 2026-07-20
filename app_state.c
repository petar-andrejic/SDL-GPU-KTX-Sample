#include "app_state.h"

#include <ktx.h>
#include <stdlib.h>
#include <SDL3/SDL_init.h>

#include "error_check.h"
#include "ktx_helpers.h"
#include "upload.h"

typedef struct Vertex2D {
    float pos[2];
    float uv[2];
} Vertex2D;

#define QUAD_VERTEX_COUNT 4

static Vertex2D vertices[QUAD_VERTEX_COUNT] = {
    {.pos = {-0.5f, +0.5f}, .uv = {0.0f, 0.0f}},
    {.pos = {+0.5f, +0.5f}, .uv = {1.0f, 0.0f}},
    {.pos = {+0.5f, -0.5f}, .uv = {1.0f, 1.0f}},
    {.pos = {-0.5f, -0.5f}, .uv = {0.0f, 1.0f}},
};

#define QUAD_INDEX_COUNT 6

static uint16_t indices[QUAD_INDEX_COUNT] = {
    0,
    1,
    2,
    2,
    3,
    0
};

SDL_GPUTexture * loadTextureFromFile(SDL_GPUDevice *device, char const *path) {
    ktxTexture *sample_img;

    SDL_SetError("Failed to load ktx texture %s", path);
    checkKTX(ktxTexture_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_NO_STORAGE, &sample_img));

    SDL_GPUTexture *tex = uploadTextureFromKTX(device, sample_img);
    checkStatus(tex);

    ktxTexture_Destroy(sample_img);
    return tex;
}

SDL_GPUSampler * createDemoSampler(SDL_GPUDevice *device) {
    SDL_GPUSamplerCreateInfo info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
        .max_anisotropy = 16.f,
        .enable_anisotropy = true,
    };
    SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &info);
    checkStatus(sampler);

    return sampler;
}

SDL_GPUGraphicsPipeline * createPipelineForMode(
    SDL_GPUDevice *device,
    SDL_GPUTextureFormat target_format,
    DemoMode mode
) {
    char const *shader_name;
    switch (mode) {
        default:
        case DEMO_MODE_2D:
            shader_name = "shaders/shader_2d.spv";
            break;
        case DEMO_MODE_3D:
            shader_name = "shaders/shader_3d.spv";
            break;
        case DEMO_MODE_ARRAY:
            shader_name = "shaders/shader_arr.spv";
            break;
    }
    size_t data_size;
    void *shader_data = SDL_LoadFile(shader_name, &data_size);
    checkStatus(shader_data);

    SDL_GPUShaderCreateInfo vertex_info = {
        .code_size = data_size,
        .code = shader_data,
        .entrypoint = "vs_main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    };

    SDL_GPUShaderCreateInfo fragment_info = {
        .code_size = data_size,
        .code = shader_data,
        .entrypoint = "fs_main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_uniform_buffers = 1,
    };

    SDL_GPUShader *vs = SDL_CreateGPUShader(device, &vertex_info);
    checkStatus(vs);

    SDL_GPUShader *fs = SDL_CreateGPUShader(device, &fragment_info);
    checkStatus(fs);

    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(Vertex2D),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };

    SDL_GPUVertexAttribute attribs[2] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(Vertex2D, pos)
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(Vertex2D, uv)
        },
    };

    SDL_GPUColorTargetDescription color_desc = {
        .format = target_format,
        .blend_state = {
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .enable_blend = true,
        },
    };
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vs,
        .fragment_shader = fs,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vb_desc,
            .num_vertex_buffers = 1,
            .vertex_attributes = attribs,
            .num_vertex_attributes = sizeof(attribs) / sizeof(attribs[0])
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        },
        .multisample_state = {
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
        },
        .depth_stencil_state = {},
        .target_info = {
            .color_target_descriptions = &color_desc,
            .num_color_targets = 1,
            .has_depth_stencil_target = false,
        },
    };

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    checkStatus(pipeline);

    SDL_ReleaseGPUShader(device, vs);
    SDL_ReleaseGPUShader(device, fs);
    free(shader_data);

    return pipeline;
}

Appstate * Appstate_create() {
    Appstate *app = malloc(sizeof(Appstate));
    SDL_SetError("Failed to allocate space for app data");
    checkStatus(app);

    checkStatus(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));

    app->window = SDL_CreateWindow("Test", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    checkStatus(app->window);

    SDL_PropertiesID prop = SDL_CreateProperties();
    checkStatus(prop);
    SDL_GPUVulkanOptions opts = {
        .vulkan_api_version = VK_API_VERSION_1_3,
    };
    SDL_SetPointerProperty(prop, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER, &opts);
    SDL_SetBooleanProperty(prop, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, APP_DEBUG_MODE);
    SDL_SetStringProperty(prop, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
    SDL_SetBooleanProperty(prop, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    app->device = SDL_CreateGPUDeviceWithProperties(prop);
    // app->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, APP_DEBUG_MODE, "vulkan");
    checkStatus(app->device);
    SDL_DestroyProperties(prop);

    checkStatus(SDL_ClaimWindowForGPUDevice(app->device, app->window));

    app->pipelines.p_2d = createPipelineForMode(
        app->device, SDL_GetGPUSwapchainTextureFormat(app->device, app->window), DEMO_MODE_2D
    );
    app->pipelines.p_3d = createPipelineForMode(
        app->device, SDL_GetGPUSwapchainTextureFormat(app->device, app->window), DEMO_MODE_3D
    );
    app->pipelines.p_arr = createPipelineForMode(
        app->device, SDL_GetGPUSwapchainTextureFormat(app->device, app->window), DEMO_MODE_ARRAY
    );
    app->textures.t_2d = loadTextureFromFile(app->device, "assets/2d_bc7.ktx2");
    app->textures.t_3d = loadTextureFromFile(app->device, "assets/3d_rgba16_linear.ktx2");
    app->textures.t_arr = loadTextureFromFile(app->device, "assets/array_rgba32_linear.ktx2");

    app->vertices = uploadToBuffer(
        app->device, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, sizeof(vertices)
    );
    checkStatus(app->vertices);

    app->indices = uploadToBuffer(app->device, SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));
    checkStatus(app->indices);

    app->sampler = createDemoSampler(app->device);

    app->selected_image = 0.0f;
    app->mode = DEMO_MODE_2D;
    return app;
}

void Appstate_destroy(Appstate *app) {
    SDL_ReleaseGPUSampler(app->device, app->sampler);
    SDL_ReleaseGPUBuffer(app->device, app->indices);
    SDL_ReleaseGPUBuffer(app->device, app->vertices);
    SDL_ReleaseGPUTexture(app->device, app->textures.t_2d);
    SDL_ReleaseGPUTexture(app->device, app->textures.t_3d);
    SDL_ReleaseGPUTexture(app->device, app->textures.t_arr);
    SDL_ReleaseGPUGraphicsPipeline(app->device, app->pipelines.p_2d);
    SDL_ReleaseGPUGraphicsPipeline(app->device, app->pipelines.p_3d);
    SDL_ReleaseGPUGraphicsPipeline(app->device, app->pipelines.p_arr);
    SDL_ReleaseWindowFromGPUDevice(app->device, app->window);
    SDL_DestroyGPUDevice(app->device);
    SDL_DestroyWindow(app->window);

    *app = (Appstate){};
}

void AppState_draw(Appstate *app) {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(app->device);
    checkStatus(cmd);

    SDL_GPUGraphicsPipeline *selected_pipeline;
    SDL_GPUTexture *selected_texture;
    switch (app->mode) {
        default:
        case DEMO_MODE_2D:
            selected_pipeline = app->pipelines.p_2d;
            selected_texture = app->textures.t_2d;
            break;
        case DEMO_MODE_3D:
            selected_pipeline = app->pipelines.p_3d;
            selected_texture = app->textures.t_3d;
            break;
        case DEMO_MODE_ARRAY:
            selected_pipeline = app->pipelines.p_arr;
            selected_texture = app->textures.t_arr;
            break;
    }
    SDL_GPUTexture *target;
    uint32_t width, height;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, app->window, &target, &width, &height);
    if (!target) goto submit;

    SDL_GPUColorTargetInfo color_target = {
        .texture = target,
        .mip_level = 0,
        .layer_or_depth_plane = 0,
        .clear_color = {.r = 1.f, .g = 1.f, .b = 1.f, .a = 1.f},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    SDL_GPUBufferBinding vb_binding = {
        .buffer = app->vertices,
    };
    SDL_BindGPUVertexBuffers(rp, 0, &vb_binding, 1);
    SDL_GPUBufferBinding ib_binding = {
        .buffer = app->indices,
    };

    SDL_BindGPUIndexBuffer(rp, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_BindGPUGraphicsPipeline(rp, selected_pipeline);
    SDL_GPUTextureSamplerBinding ts_binding = {
        .texture = selected_texture,
        .sampler = app->sampler
    };
    SDL_BindGPUFragmentSamplers(rp, 0, &ts_binding, 1);
    SDL_PushGPUFragmentUniformData(cmd, 0, &app->selected_image, sizeof(float));
    SDL_DrawGPUIndexedPrimitives(rp, QUAD_INDEX_COUNT, 1, 0, 0, 0);
    SDL_EndGPURenderPass(rp);

submit:
    SDL_SubmitGPUCommandBuffer(cmd);
}

void AppState_onKeyDown(Appstate *app, SDL_KeyboardEvent *key) {
    switch (key->key) {
        case SDLK_1:
            app->selected_image = 0.0f;
            break;
        case SDLK_2:
            app->selected_image = 1.0f;
            break;
        case SDLK_3:
            app->selected_image = 2.0f;
            break;
        case SDLK_Q:
            app->mode = DEMO_MODE_2D;
            app->selected_image = 0.0f;
            break;
        case SDLK_W:
            app->mode = DEMO_MODE_3D;
            app->selected_image = 0.0f;
            break;
        case SDLK_E:
            app->mode = DEMO_MODE_ARRAY;
            app->selected_image = 0.0f;
            break;
        default:
            break;
    }
    float increment = app->mode == DEMO_MODE_3D ? 0.15f : 1.f;
    app->selected_image *= increment;
}

void Appstate_handleEvents(Appstate *app, SDL_Event *event) {
    switch (event->type) {
        default:
            return;
        case SDL_EVENT_KEY_DOWN:
            return AppState_onKeyDown(app, &event->key);
    }
}
