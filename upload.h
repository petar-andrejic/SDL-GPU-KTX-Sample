#pragma once

#include <SDL3/SDL_gpu.h>

typedef bool (*PFn_UploadHandler)(void *mapped_data, void *data_handle, size_t data_size);

SDL_GPUTransferBuffer *uploadToStaging(SDL_GPUDevice *device, void *data, size_t data_size);

SDL_GPUTransferBuffer *uploadToStagingEx(
    SDL_GPUDevice *device,
    void *data_handle,
    size_t data_size,
    PFn_UploadHandler handler
);

SDL_GPUBuffer *uploadToBuffer(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    void *data,
    size_t data_size
);

SDL_GPUBuffer *uploadToBufferEx(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    void *data,
    size_t data_size,
    PFn_UploadHandler handler
);
