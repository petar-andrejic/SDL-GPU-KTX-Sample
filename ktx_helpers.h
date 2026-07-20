#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_gpu.h>
#include <ktx.h>

bool getTextureTypeFromKTX(ktxTexture *ktx_texture, SDL_GPUTextureType *texture_type);

bool getLayerCountOrDepthFromKTX(ktxTexture *ktx_texture, uint32_t* count);

bool getGPUTextureFormatFromKTX(ktxTexture *ktx_texture, SDL_GPUTextureFormat* format);

// Obtain the `SDL_GPUTextureFormat` for a given `VkFormat`. If a format is not supported,
// returns SDL_GPU_TEXTUREFORMAT_INVALID
SDL_GPUTextureFormat convertVkFormatToSDL(VkFormat format);

SDL_GPUTransferBuffer* uploadStagingFromKTX(SDL_GPUDevice* device, ktxTexture* ktx_texture);

SDL_GPUTexture* uploadTextureFromKTX(SDL_GPUDevice* device, ktxTexture* ktx_texture);


