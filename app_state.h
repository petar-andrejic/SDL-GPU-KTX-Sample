#pragma once

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_events.h>

typedef enum DemoMode {
    DEMO_MODE_2D,
    DEMO_MODE_3D,
    DEMO_MODE_ARRAY
} DemoMode;

typedef struct AppState {
    SDL_Window *window;
    SDL_GPUDevice *device;

    struct {
        SDL_GPUGraphicsPipeline *p_2d;
        SDL_GPUGraphicsPipeline *p_3d;
        SDL_GPUGraphicsPipeline *p_arr;
    } pipelines;

    struct {
        SDL_GPUTexture *t_2d;
        SDL_GPUTexture *t_3d;
        SDL_GPUTexture *t_arr;
    } textures;

    SDL_GPUBuffer *vertices;
    SDL_GPUBuffer *indices;
    SDL_GPUSampler *sampler;
    float selected_image;
    DemoMode mode;
} Appstate;

Appstate *Appstate_create();

void Appstate_destroy(Appstate *app);

void AppState_draw(Appstate *app);

void AppState_onKeyDown(Appstate *app, SDL_KeyboardEvent *key);

void Appstate_handleEvents(Appstate *app, SDL_Event *event);
