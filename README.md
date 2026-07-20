# SDL3 KTX texture sample

This repository contains a simple sample of loading and displaying a KTX texture using the [SDL3](https://github.com/libsdl-org/SDL) GPU API and [libktx](https://github.khronos.org/KTX-Software/libktx/index.html)

At the moment there is no easy way to get an SDL_GPUTextureFormat from the format of the KTX texture without going through the Vulkan API first, so this sample does that. The easiest way around this would be sticking to a known format, and transcoding textures to this format. 

# Building

Building uses CMake, and requires having SDL3, Vulkan and libktx visible to `find_package`

Additionally, the shaders require the [slang](https://shader-slang.org/) compiler, and setting the 
CMake variable `SLANGC_EXECUTABLE`  to point to your copy of `slangc`.

# Usage

Press `1` `2` or `3` to change which image in the 2D array or 3D texture is displayed

Press `Q` `W` or `E` to change between 2D, 3D or 2D array texture mode respectively

# Things to note

When loading supercompressed textures, libktx will deflate them. Make sure to use `ktxTexture_GetDataSizeUncompressed` to size your GPU buffers. libktx lets you upload the texture directly from disk without needing to go to an intermediate CPU buffer. To do this, use `KTX_TEXTURE_CREATE_NO_STORAGE` when creating the `ktxTexture`, and use `ktxTexture_LoadImageData` directly targeting the mapped transfer buffer data.

# Todo

- [ ] Add cube map sample 
- [ ] Verify if row padding is an issue with SDL3 or not. 
- [ ] If not too much of a pain, implement helper function to obtain `SDL3_GPUTextureFormat` from `ktxTexture`
- [ ] Test on DirectX and Metal
