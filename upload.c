#include "upload.h"

#include <SDL3/SDL_log.h>

SDL_GPUTransferBuffer *uploadToStagingEx(
    SDL_GPUDevice *device,
    void *data_handle,
    size_t data_size,
    PFn_UploadHandler handler
) {
    SDL_GPUTransferBufferCreateInfo staging_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = data_size,
    };
    SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(device, &staging_info);
    if (!staging) goto create_staging_fail;

    void *mapped_data = SDL_MapGPUTransferBuffer(device, staging, false);
    if (!mapped_data) goto map_staging_fail;

    if (!handler(mapped_data, data_handle, data_size)) goto upload_fail;

    SDL_UnmapGPUTransferBuffer(device, staging);
    return staging;

upload_fail:
    SDL_UnmapGPUTransferBuffer(device, staging);
map_staging_fail:
    SDL_ReleaseGPUTransferBuffer(device, staging);
create_staging_fail:
    return NULL;
}

bool uploadHandlerMemcpy(void* mapped_data, void* data, size_t data_size) {
    memcpy(mapped_data, data, data_size);
    return true;
}

SDL_GPUTransferBuffer *uploadToStaging(SDL_GPUDevice *device, void *data, size_t data_size) {
    return uploadToStagingEx(device, data, data_size, &uploadHandlerMemcpy);
}

SDL_GPUBuffer *uploadToBuffer(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    void *data,
    size_t data_size
) {
    return uploadToBufferEx(device, usage, data, data_size, &uploadHandlerMemcpy);
};

SDL_GPUBuffer *uploadToBufferEx(
    SDL_GPUDevice *device,
    SDL_GPUBufferUsageFlags usage,
    void *data,
    size_t data_size,
    PFn_UploadHandler handler
) {
    SDL_GPUTransferBuffer *staging = uploadToStagingEx(device, data, data_size, handler);
    if (!staging) goto staging_fail;

    SDL_GPUBufferCreateInfo buf_info = {
        .usage = usage,
        .size = data_size,
    };
    SDL_GPUBuffer *buf = SDL_CreateGPUBuffer(device, &buf_info);
    if (!buf) goto buffer_create_fail;

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) goto command_acquire_fail;


    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {
        .transfer_buffer = staging,
    };
    SDL_GPUBufferRegion dst = {
        .buffer = buf,
        .size = data_size
    };

    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!SDL_WaitForGPUFences(device, true, &fence, 1)) goto sync_fail;

    SDL_ReleaseGPUFence(device, fence);
    SDL_ReleaseGPUTransferBuffer(device, staging);
    return buf;
sync_fail:
    SDL_ReleaseGPUFence(device, fence);
command_acquire_fail:
    SDL_ReleaseGPUBuffer(device, buf);
buffer_create_fail:
    SDL_ReleaseGPUTransferBuffer(device, staging);
staging_fail:
    return NULL;
};
