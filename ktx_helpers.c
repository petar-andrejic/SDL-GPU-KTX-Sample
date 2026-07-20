#include "ktx_helpers.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <ktxvulkan.h>
#include <math.h>

#include "error_check.h"
#include "upload.h"

bool getTextureTypeFromKTX(ktxTexture *ktx_texture, SDL_GPUTextureType *texture_type) {
    if (ktx_texture->isCubemap) {
        if (ktx_texture->isArray) {
            *texture_type = SDL_GPU_TEXTURETYPE_CUBE_ARRAY;
        } else {
            *texture_type = SDL_GPU_TEXTURETYPE_CUBE;
        }
        return true;
    }

    if (ktx_texture->numDimensions == 3 && !ktx_texture->isArray) {
        *texture_type = SDL_GPU_TEXTURETYPE_3D;
        return true;
    }

    if (ktx_texture->numDimensions == 2) {
        if (ktx_texture->isArray) {
            *texture_type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
        } else {
            *texture_type = SDL_GPU_TEXTURETYPE_2D;
        }

        return true;
    }

    return SDL_SetError(
        "Unsupported combination of ktx texture features:\n"
        "\tisCubemap = %i\n"
        "\tisArray = %i\n"
        "\tnumDimensions = %i\n",
        ktx_texture->isCubemap, ktx_texture->isArray, ktx_texture->numDimensions
    );
}

bool getLayerCountOrDepthFromKTX(ktxTexture *ktx_texture, uint32_t *count) {
    if (ktx_texture->isCubemap) {
        if (ktx_texture->isArray) {
            return SDL_SetError("Cubemap arrays have not been implemented by this library yet");
        } else {
            *count = ktx_texture->numFaces;
        }
    } else if (ktx_texture->numDimensions == 3) {
        if (ktx_texture->isArray) {
            return SDL_SetError("3D texture arrays are not supported by SDL GPU");
        } else {
            *count = ktx_texture->baseDepth;
        }
    } else if (ktx_texture->numDimensions == 2) {
        if (ktx_texture->isArray) {
            *count = ktx_texture->numLayers;
        } else {
            *count = ktx_texture->baseDepth;
        }
    } else {
        return SDL_SetError("1D Textures are not supported by SDL GPU");
    }

    return true;
}

bool getGPUTextureFormatFromKTX(ktxTexture *ktx_texture, SDL_GPUTextureFormat* result) {
    VkFormat vk_format = ktxTexture_GetVkFormat(ktx_texture);
    *result = convertVkFormatToSDL(vk_format);

    if (*result == SDL_GPU_TEXTUREFORMAT_INVALID)
        return SDL_SetError("Unsupported VkFormat %s", string_VkFormat(vk_format));

    return true;
}

bool uploadHandlerKTX(void *mapped_data, ktxTexture *ktx_texture, size_t data_size) {
    ktx_error_code_e ktx_result = ktxTexture_LoadImageData(
        ktx_texture,
        mapped_data,
        data_size // ktx wants the target buffer size, which we have provided as an arg
    );
    if (ktx_result != KTX_SUCCESS)
        return SDL_SetError("Failed to load image data\n%s\n", ktxErrorString(ktx_result));

    return true;
}

SDL_GPUTransferBuffer *uploadToStagingBufferKTX(SDL_GPUDevice *device, ktxTexture *ktx_texture) {
    size_t data_size = ktxTexture_GetDataSizeUncompressed(ktx_texture);
    return uploadToStagingEx(
        device, ktx_texture, data_size,
        (PFn_UploadHandler) &uploadHandlerKTX
    );
}

uint32_t u32_max(uint32_t lhs, uint32_t rhs) {
    if (lhs < rhs)
        return rhs;

    return lhs;
}

typedef ktx_error_code_e (*PFn_ImageIterCb)(
    uint32_t miplevel,
    uint32_t layer,
    uint32_t slice, // face index for cube maps, depth slice otherwise
    uint32_t width,
    uint32_t height,
    void *user_data
);


typedef struct CopyImageUserData {
    ktxTexture *img;
    SDL_GPUCopyPass *copy_pass;
    SDL_GPUTransferBuffer *transfer_buffer;
    SDL_GPUTexture *texture;
} CopyImageUserData;


ktx_error_code_e copyImage(
    uint32_t miplevel,
    uint32_t layer,
    uint32_t slice,
    uint32_t width,
    uint32_t height,
    CopyImageUserData *ud
) {
    size_t offset;
    ktx_error_code_e result = ktxTexture_GetImageOffset(ud->img, miplevel, layer, slice, &offset);
    if (result != KTX_SUCCESS) return result;

    SDL_GPUTextureTransferInfo source_info = {
        .transfer_buffer = ud->transfer_buffer,
        .offset = offset,
    };
    SDL_GPUTextureRegion dest_info = {
        .texture = ud->texture,
        .mip_level = miplevel,
        .layer = layer,
        .z = slice,
        .w = width,
        .h = height,
        .d = 1, // we are uploading slice by slice
    };
    SDL_UploadToGPUTexture(ud->copy_pass, &source_info, &dest_info, false);

    return KTX_SUCCESS;
}

ktx_error_code_e iterKTXImages(ktxTexture *tex, PFn_ImageIterCb iter_cb, void *user_data) {
    for (uint32_t mip_level = 0; mip_level < tex->numLevels; ++mip_level) {
        uint32_t width = u32_max(1, tex->baseWidth >> mip_level);
        uint32_t height = u32_max(1, tex->baseHeight >> mip_level);
        uint32_t depth = u32_max(1, tex->baseDepth >> mip_level);

        uint32_t num_slices = tex->isCubemap ? tex->numFaces : depth;
        for (uint32_t slice = 0; slice < num_slices; ++slice) {
            for (uint32_t layer = 0; layer < tex->numLayers; ++layer) {
                ktx_error_code_e result = iter_cb(mip_level, layer, slice, width, height, user_data);
                if (result != KTX_SUCCESS) {
                    return result;
                }
            }
        }
    }

    return KTX_SUCCESS;
}

SDL_GPUTexture *uploadTextureFromKTX(SDL_GPUDevice *device, ktxTexture *ktx_texture) {
    SDL_GPUTextureFormat format;
    if (!getGPUTextureFormatFromKTX(ktx_texture, &format))
        goto get_format_fail;

    ktx_error_code_e ktx_result;
    SDL_GPUTextureType tex_type;
    if (!getTextureTypeFromKTX(ktx_texture, &tex_type))
        goto get_type_fail;

    uint32_t layer_count_or_depth;
    if (!getLayerCountOrDepthFromKTX(ktx_texture, &layer_count_or_depth))
        goto get_layer_count_fail;


    SDL_GPUTextureCreateInfo tex_info = {
        .type = tex_type,
        .format = format,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = ktx_texture->baseWidth,
        .height = ktx_texture->baseHeight,
        .layer_count_or_depth = layer_count_or_depth,
        .num_levels = ktx_texture->numLevels,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex)
        goto create_tex_fail;

    SDL_GPUTransferBuffer *staging = uploadToStagingBufferKTX(device, ktx_texture);
    if (!staging)
        goto staging_fail;

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd)
        goto acquire_cmd_fail;

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    CopyImageUserData user_data = {
        .img = ktx_texture,
        .copy_pass = copy_pass,
        .transfer_buffer = staging,
        .texture = tex
    };
    ktx_result = iterKTXImages(
        ktx_texture, (PFn_ImageIterCb) &copyImage, &user_data
    );
    if (ktx_result != KTX_SUCCESS) {
        SDL_SetError(
            "Failed to upload to texture from staging buffer\n%s\n",
            ktxErrorString(ktx_result)
        );
        goto upload_fail;
    }
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, staging);

    return tex;
upload_fail:
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);
acquire_cmd_fail:
    SDL_ReleaseGPUTransferBuffer(device, staging);
staging_fail:
    SDL_ReleaseGPUTexture(device, tex);
create_tex_fail:
get_layer_count_fail:
get_type_fail:
get_format_fail:
    return NULL;
}


SDL_GPUTextureFormat convertVkFormatToSDL(VkFormat format) {
    switch (format) {
        default:
            return SDL_GPU_TEXTUREFORMAT_INVALID;
        case VK_FORMAT_A8_UNORM:
            return SDL_GPU_TEXTUREFORMAT_A8_UNORM;
        case VK_FORMAT_R8_UNORM:
            return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        case VK_FORMAT_R8G8_UNORM:
            return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_R16_UNORM:
            return SDL_GPU_TEXTUREFORMAT_R16_UNORM;
        case VK_FORMAT_R16G16_UNORM:
            return SDL_GPU_TEXTUREFORMAT_R16G16_UNORM;
        case VK_FORMAT_R16G16B16A16_UNORM:
            return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
            return SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM;
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
            return SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            return SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
        case VK_FORMAT_BC2_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
        case VK_FORMAT_BC3_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
        case VK_FORMAT_BC4_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM;
        case VK_FORMAT_BC5_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM;
        case VK_FORMAT_BC7_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT;
        case VK_FORMAT_R8_SNORM:
            return SDL_GPU_TEXTUREFORMAT_R8_SNORM;
        case VK_FORMAT_R8G8_SNORM:
            return SDL_GPU_TEXTUREFORMAT_R8G8_SNORM;
        case VK_FORMAT_R8G8B8A8_SNORM:
            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM;
        case VK_FORMAT_R16_SNORM:
            return SDL_GPU_TEXTUREFORMAT_R16_SNORM;
        case VK_FORMAT_R16G16_SNORM:
            return SDL_GPU_TEXTUREFORMAT_R16G16_SNORM;
        case VK_FORMAT_R16G16B16A16_SNORM:
            return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM;
        case VK_FORMAT_R16_SFLOAT:
            return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
        case VK_FORMAT_R16G16_SFLOAT:
            return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        case VK_FORMAT_R32_SFLOAT:
            return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        case VK_FORMAT_R32G32_SFLOAT:
            return SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            return SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
        case VK_FORMAT_R8_UINT:
            return SDL_GPU_TEXTUREFORMAT_R8_UINT;
        case VK_FORMAT_R8G8_UINT:
            return SDL_GPU_TEXTUREFORMAT_R8G8_UINT;
        case VK_FORMAT_R8G8B8A8_UINT:
            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT;
        case VK_FORMAT_R16_UINT:
            return SDL_GPU_TEXTUREFORMAT_R16_UINT;
        case VK_FORMAT_R16G16_UINT:
            return SDL_GPU_TEXTUREFORMAT_R16G16_UINT;
        case VK_FORMAT_R16G16B16A16_UINT:
            return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT;
        case VK_FORMAT_R32_UINT:
            return SDL_GPU_TEXTUREFORMAT_R32_UINT;
        case VK_FORMAT_R32G32_UINT:
            return SDL_GPU_TEXTUREFORMAT_R32G32_UINT;
        case VK_FORMAT_R32G32B32A32_UINT:
            return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT;
        case VK_FORMAT_R8_SINT:
            return SDL_GPU_TEXTUREFORMAT_R8_INT;
        case VK_FORMAT_R8G8_SINT:
            return SDL_GPU_TEXTUREFORMAT_R8G8_INT;
        case VK_FORMAT_R8G8B8A8_SINT:
            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT;
        case VK_FORMAT_R16_SINT:
            return SDL_GPU_TEXTUREFORMAT_R16_INT;
        case VK_FORMAT_R16G16_SINT:
            return SDL_GPU_TEXTUREFORMAT_R16G16_INT;
        case VK_FORMAT_R16G16B16A16_SINT:
            return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT;
        case VK_FORMAT_R32_SINT:
            return SDL_GPU_TEXTUREFORMAT_R32_INT;
        case VK_FORMAT_R32G32_SINT:
            return SDL_GPU_TEXTUREFORMAT_R32G32_INT;
        case VK_FORMAT_R32G32B32A32_SINT:
            return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC2_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC3_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB;
        case VK_FORMAT_D16_UNORM:
            return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return SDL_GPU_TEXTUREFORMAT_D24_UNORM;
        case VK_FORMAT_D32_SFLOAT:
            return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM;
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM;
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM;
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM;
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM;
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM;
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM;
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM;
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM;
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM;
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM;
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM;
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB;
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM_SRGB;
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM_SRGB;
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM_SRGB;
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM_SRGB;
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM_SRGB;
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM_SRGB;
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM_SRGB;
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM_SRGB;
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM_SRGB;
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM_SRGB;
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM_SRGB;
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM_SRGB;
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM_SRGB;
        case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_4x4_FLOAT;
        case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_5x4_FLOAT;
        case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_5x5_FLOAT;
        case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_6x5_FLOAT;
        case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_6x6_FLOAT;
        case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x5_FLOAT;
        case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x6_FLOAT;
        case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_8x8_FLOAT;
        case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x5_FLOAT;
        case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x6_FLOAT;
        case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x8_FLOAT;
        case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_10x10_FLOAT;
        case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT:
            return SDL_GPU_TEXTUREFORMAT_ASTC_12x10_FLOAT;
        case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
            return SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT;
    };
}
