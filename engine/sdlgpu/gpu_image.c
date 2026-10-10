/* Texture upload, lifetime and filtering for SDL3 GPU. GPL-2.0-or-later. */
#include "gpu_local.h"

static int filtermip[3] = {1, 1, 1}, filterpic[3] = {1, 1, 1};
static int mipcap[2] = {0, 1000};
static float samplerbias, sampleranisotropy = 1;

SDL_GPUTextureFormat GPU_TextureFormat(uploadfmt_t fmt)
{
    switch (fmt) {
    case PTI_RGBA8: case PTI_RGBX8: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case PTI_BGRA8: case PTI_BGRX8: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case PTI_RGBA8_SRGB: case PTI_RGBX8_SRGB: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case PTI_BGRA8_SRGB: case PTI_BGRX8_SRGB: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    case PTI_R8: return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    case PTI_RGBA16F: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case PTI_RGBA32F: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case PTI_DEPTH16: return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    case PTI_DEPTH32: return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    case PTI_BC1_RGB: case PTI_BC1_RGBA: return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
    case PTI_BC2_RGBA: return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
    case PTI_BC3_RGBA: return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
    case PTI_BC4_R: return SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM;
    case PTI_BC5_RG: return SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM;
    case PTI_BC6_RGB_UFLOAT: return SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT;
    case PTI_BC7_RGBA: return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
    default: return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

void GPU_InitTextureFormats(void)
{
    unsigned i;
    for (i = 0; i < PTI_MAX; ++i) {
        SDL_GPUTextureFormat fmt = GPU_TextureFormat(i);
        sh_config.texfmt[i] = fmt != SDL_GPU_TEXTUREFORMAT_INVALID &&
            SDL_GPUTextureSupportsFormat(gpu.device, fmt, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    }
    /* Luminance needs RGB replication; the upload path expands it explicitly. */
    sh_config.texfmt[PTI_L8] = true;
    sh_config.texture2d_maxsize = 16384;
    sh_config.texturecube_maxsize = 16384;
    sh_config.texture3d_maxsize = 2048;
    sh_config.texture2darray_maxlayers = 256;
    sh_config.texture_non_power_of_two = true;
    sh_config.texture_non_power_of_two_pic = true;
    sh_config.texture_allow_block_padding = true;
    sh_config.havecubemaps = true;
    sh_config.can_mipcap = true;
    sh_config.can_mipbias = true;
}

static void GPU_CreateImageSampler(image_t *tex)
{
    gpu_image_t *img = tex->gpuimage;
    SDL_GPUSamplerCreateInfo info = {0};
    const int *filter = (tex->flags & IF_UIPIC) ? filterpic : filtermip;
    if (tex->flags & IF_NEAREST) info.min_filter = info.mag_filter = SDL_GPU_FILTER_NEAREST;
    else if (tex->flags & IF_LINEAR) info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
    else {
        info.min_filter = filter[0] ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        info.mag_filter = filter[2] ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    }
    info.mipmap_mode = filter[1] ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = info.address_mode_v = info.address_mode_w = (tex->flags & IF_CLAMP) ?
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.mip_lod_bias = samplerbias;
    info.max_lod = min(mipcap[1], img->levels - 1);
    info.min_lod = bound(0, mipcap[0], info.max_lod);
    info.max_anisotropy = bound(1, sampleranisotropy, 16);
    info.enable_anisotropy = !(tex->flags & IF_NEAREST) && info.max_anisotropy > 1;
    if (img->sampler) SDL_ReleaseGPUSampler(gpu.device, img->sampler);
    img->sampler = SDL_CreateGPUSampler(gpu.device, &info);
    if (!img->sampler) GPU_Error("create image sampler");
}

void GPU_UpdateFiltering(image_t *images, int mip[3], int pic[3], int cap[2], float bias, float anis)
{
    memcpy(filtermip, mip, sizeof(filtermip)); memcpy(filterpic, pic, sizeof(filterpic));
    memcpy(mipcap, cap, sizeof(mipcap)); samplerbias = bias; sampleranisotropy = anis;
    for (; images; images = images->next)
        if (images->gpuimage) GPU_CreateImageSampler(images);
}

void GPU_DestroyTexture(texid_t tex)
{
    if (!tex || !tex->gpuimage) return;
    if (tex->gpuimage->texture) SDL_ReleaseGPUTexture(gpu.device, tex->gpuimage->texture);
    if (tex->gpuimage->sampler) SDL_ReleaseGPUSampler(gpu.device, tex->gpuimage->sampler);
    Z_Free(tex->gpuimage); tex->gpuimage = NULL;
}

qboolean GPU_LoadTextureMips(texid_t tex, const struct pendingtextureinfo *mips)
{
    SDL_GPUTextureCreateInfo ci = {0};
    SDL_GPUTransferBufferCreateInfo ti = {0};
    SDL_GPUTransferBuffer *transfer;
    SDL_GPUCommandBuffer *command;
    SDL_GPUCopyPass *copy;
    gpu_image_t *img;
    unsigned i, offset = 0, levels, layers, bytes;
    qbyte *mapped;
    qboolean luminance = mips->encoding == PTI_L8;
    qboolean xalpha = mips->encoding == PTI_RGBX8 || mips->encoding == PTI_BGRX8 ||
        mips->encoding == PTI_RGBX8_SRGB || mips->encoding == PTI_BGRX8_SRGB;
    if (!mips->mipcount) return false;
    ci.format = luminance ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM : GPU_TextureFormat(mips->encoding);
    if (ci.format == SDL_GPU_TEXTUREFORMAT_INVALID) return false;
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    layers = 1;
    if (mips->type == PTI_CUBE) { ci.type = SDL_GPU_TEXTURETYPE_CUBE; layers = 6; }
    else if (mips->type == PTI_2D_ARRAY) { ci.type = SDL_GPU_TEXTURETYPE_2D_ARRAY; layers = mips->mip[0].depth; }
    else if (mips->type == PTI_CUBE_ARRAY) { ci.type = SDL_GPU_TEXTURETYPE_CUBE_ARRAY; layers = mips->mip[0].depth; }
    else if (mips->type == PTI_3D) { ci.type = SDL_GPU_TEXTURETYPE_3D; }
    else if (mips->type != PTI_2D) return false;
    levels = mips->mipcount;
    if (!levels) return false;
    ci.width = mips->mip[0].width; ci.height = mips->mip[0].height;
    ci.layer_count_or_depth = ci.type == SDL_GPU_TEXTURETYPE_3D ? mips->mip[0].depth : layers;
    ci.num_levels = levels; ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if (tex->flags & IF_RENDERTARGET)
        ci.usage |= mips->encoding == PTI_DEPTH16 || mips->encoding == PTI_DEPTH32 ?
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET : SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    img = tex->gpuimage;
    if (!img || img->format != ci.format || img->width != ci.width || img->height != ci.height ||
        img->levels != levels || img->layers != ci.layer_count_or_depth || img->type != ci.type || img->usage != ci.usage) {
        GPU_DestroyTexture(tex);
        img = tex->gpuimage = Z_Malloc(sizeof(*img));
        img->format = ci.format; img->width = ci.width; img->height = ci.height;
        img->levels = levels; img->layers = ci.layer_count_or_depth; img->type = ci.type; img->usage = ci.usage;
        img->texture = SDL_CreateGPUTexture(gpu.device, &ci);
        if (!img->texture) { GPU_DestroyTexture(tex); return false; }
        GPU_CreateImageSampler(tex);
    }
    for (i = 0; i < mips->mipcount; ++i) {
        if (!mips->mip[i].data) continue;
        bytes = luminance ? mips->mip[i].width*mips->mip[i].height*max(1, mips->mip[i].depth)*4 : mips->mip[i].datasize;
        offset = (offset + 15) & ~15u; offset += bytes;
    }
    if (!offset) return true; /* Render targets can be allocated without initial contents. */
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; ti.size = offset;
    transfer = SDL_CreateGPUTransferBuffer(gpu.device, &ti);
    if (!transfer) return false;
    mapped = SDL_MapGPUTransferBuffer(gpu.device, transfer, false);
    if (!mapped) { SDL_ReleaseGPUTransferBuffer(gpu.device, transfer); return false; }
    offset = 0;
    for (i = 0; i < mips->mipcount; ++i) {
        unsigned j;
        if (!mips->mip[i].data) continue;
        offset = (offset + 15) & ~15u;
        bytes = luminance ? mips->mip[i].width*mips->mip[i].height*max(1, mips->mip[i].depth)*4 : mips->mip[i].datasize;
        if (luminance) for (j = 0; j < bytes/4; ++j) {
            mapped[offset+j*4] = mapped[offset+j*4+1] = mapped[offset+j*4+2] = ((qbyte *)mips->mip[i].data)[j];
            mapped[offset+j*4+3] = 255;
        } else {
            memcpy(mapped+offset, mips->mip[i].data, bytes);
            if (xalpha) for (j = 3; j < bytes; j += 4) mapped[offset+j] = 255;
        }
        offset += bytes;
    }
    SDL_UnmapGPUTransferBuffer(gpu.device, transfer);
    command = SDL_AcquireGPUCommandBuffer(gpu.device);
    if (!command) { SDL_ReleaseGPUTransferBuffer(gpu.device, transfer); return false; }
    copy = SDL_BeginGPUCopyPass(command);
    offset = 0;
    for (i = 0; i < mips->mipcount; ++i) {
        SDL_GPUTextureTransferInfo src = {0}; SDL_GPUTextureRegion dst = {0};
        if (!mips->mip[i].data) continue;
        offset = (offset + 15) & ~15u;
        src.transfer_buffer = transfer; src.offset = offset;
        dst.texture = img->texture; dst.mip_level = i;
        dst.w = mips->mip[i].width; dst.h = mips->mip[i].height;
        dst.d = ci.type == SDL_GPU_TEXTURETYPE_3D ? mips->mip[i].depth : 1;
        bytes = luminance ? dst.w*dst.h*max(1, mips->mip[i].depth)*4 : mips->mip[i].datasize;
        if (layers > 1) {
            unsigned layer;
            for (layer = 0; layer < layers; ++layer) {
                dst.layer = layer;
                src.offset = offset + layer * (bytes / layers);
                SDL_UploadToGPUTexture(copy, &src, &dst, false);
            }
        } else SDL_UploadToGPUTexture(copy, &src, &dst, false);
        offset += bytes;
    }
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(command)) GPU_Error("submit texture upload");
    SDL_ReleaseGPUTransferBuffer(gpu.device, transfer);
    return true;
}

SDL_GPUTextureSamplerBinding GPU_TextureBinding(texid_t tex, texid_t fallback)
{
    SDL_GPUTextureSamplerBinding binding = {0};
    if (!tex || !tex->gpuimage || tex->status != TEX_LOADED) tex = fallback;
    if (tex && tex->gpuimage) {
        binding.texture = tex->gpuimage->texture; binding.sampler = tex->gpuimage->sampler;
    }
    return binding;
}

void GPU_UploadLightmaps(qboolean force)
{
    unsigned i;
    for (i = 0; i < numlightmaps; ++i) {
        lightmapinfo_t *lm = lightmap[i];
        struct pendingtextureinfo mips = {0};
        if (!lm || lm->external || (!force && !lm->modified)) continue;
        if (!lm->lightmap_texture)
            lm->lightmap_texture = Image_CreateTexture("***lightmap***", NULL, IF_LINEAR | IF_CLAMP | IF_NOMIPMAP);
        mips.type = PTI_2D; mips.encoding = lm->fmt; mips.mipcount = 1;
        if (mips.encoding == PTI_BGRA8) mips.encoding = PTI_BGRX8;
        mips.mip[0].data = lm->lightmaps; mips.mip[0].width = lm->width;
        mips.mip[0].height = lm->height; mips.mip[0].depth = 1;
        mips.mip[0].datasize = lm->width*lm->height*lm->pixbytes;
        if (GPU_LoadTextureMips(lm->lightmap_texture, &mips)) {
            lm->lightmap_texture->status = TEX_LOADED;
            lm->lightmap_texture->width = lm->width; lm->lightmap_texture->height = lm->height;
            lm->modified = false;
            lm->rectchange.l = lm->width; lm->rectchange.t = lm->height;
            lm->rectchange.r = lm->rectchange.b = 0;
        }
    }
}
