/* Texture upload, lifetime and filtering for SDL3 GPU. GPL-2.0-or-later. */
#include "gpu_local.h"

static int filtermip[3] = {1, 1, 1}, filterpic[3] = {1, 1, 1};
static int mipcap[2] = {0, 1000};
static float samplerbias, sampleranisotropy = 1;
static size_t texturebytes;
static cvar_t gpu_lightmap_partial = CVARD("gpu_lightmap_partial", "1", "Upload only changed lightmap regions. Zero uploads whole lightmaps for comparison.");

size_t GPU_TextureBytes(void) { return texturebytes; }
size_t GPU_ImageBytes(texid_t tex) { return tex && tex->gpuimage ? tex->gpuimage->bytes : 0; }

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
    if (Cvar_FindVar(gpu_lightmap_partial.name) != &gpu_lightmap_partial)
        Cvar_Register(&gpu_lightmap_partial, "SDL GPU");
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
    info.max_lod = bound(0, mipcap[1]-(int)img->firstmip, (int)img->levels-1);
    info.min_lod = bound(0, mipcap[0]-(int)img->firstmip, info.max_lod);
    info.max_anisotropy = bound(1, sampleranisotropy, 16);
    info.enable_anisotropy = !(tex->flags & IF_NEAREST) && info.max_anisotropy > 1;
    if (img->sampler) SDL_ReleaseGPUSampler(gpu.device, img->sampler);
    img->sampler = SDL_CreateGPUSampler(gpu.device, &info);
    if (!img->sampler) GPU_Error("create image sampler");
}

void GPU_SetImageMip(texid_t tex, unsigned firstmip)
{
    if (tex->gpuimage && tex->gpuimage->firstmip != firstmip) {
        tex->gpuimage->firstmip = firstmip;
        GPU_CreateImageSampler(tex);
    }
}

void GPU_UpdateFiltering(image_t *images, int mip[3], int pic[3], int cap[2], float bias, float anis)
{
    memcpy(filtermip, mip, sizeof(filtermip)); memcpy(filterpic, pic, sizeof(filterpic));
    memcpy(mipcap, cap, sizeof(mipcap)); samplerbias = bias; sampleranisotropy = anis;
    for (; images; images = images->next)
        if (images->gpuimage) GPU_CreateImageSampler(images);
}

static void GPU_FreeImage(texid_t tex)
{
    if (!tex || !tex->gpuimage) return;
    texturebytes -= tex->gpuimage->bytes;
    if (tex->gpuimage->texture) SDL_ReleaseGPUTexture(gpu.device, tex->gpuimage->texture);
    if (tex->gpuimage->sampler) SDL_ReleaseGPUSampler(gpu.device, tex->gpuimage->sampler);
    Z_Free(tex->gpuimage); tex->gpuimage = NULL;
}

void GPU_DestroyTexture(texid_t tex)
{
    if (!tex) return;
    GPU_StreamDestroy(tex);
    GPU_FreeImage(tex);
}

qboolean GPU_UploadTextureMips(texid_t tex, const struct pendingtextureinfo *mips)
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
        GPU_FreeImage(tex);
        img = tex->gpuimage = Z_Malloc(sizeof(*img));
        img->format = ci.format; img->width = ci.width; img->height = ci.height;
        img->levels = levels; img->layers = ci.layer_count_or_depth; img->type = ci.type; img->usage = ci.usage;
        img->texture = SDL_CreateGPUTexture(gpu.device, &ci);
        if (!img->texture) { GPU_FreeImage(tex); return false; }
        for (i = 0; i < levels; ++i) {
            unsigned bw, bh, bd, blockbytes;
            Image_BlockSizeForEncoding(luminance ? PTI_RGBA8 : mips->encoding, &blockbytes, &bw, &bh, &bd);
            img->bytes += (size_t)((mips->mip[i].width+bw-1)/bw) *
                ((mips->mip[i].height+bh-1)/bh) * ((max(1,mips->mip[i].depth)+bd-1)/bd) * blockbytes;
        }
        texturebytes += img->bytes;
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
    GPU_StreamBind(tex);
    if (tex && tex->gpuimage) {
        binding.texture = tex->gpuimage->texture; binding.sampler = tex->gpuimage->sampler;
    }
    return binding;
}

typedef struct {
    lightmapinfo_t *lightmap;
    SDL_GPUTextureRegion dst;
    Uint32 offset, bytes;
    qboolean opaque;
} gpu_lightmap_region_t;

static qboolean GPU_LightmapRegion(lightmapinfo_t *lm, uploadfmt_t encoding, gpu_lightmap_region_t *region)
{
    const image_t *tex = lm->lightmap_texture;
    const gpu_image_t *img = tex ? tex->gpuimage : NULL;
    glRect_t rect = lm->rectchange;
    qboolean opaque = encoding == PTI_BGRX8 || encoding == PTI_RGBX8;

    if ((!opaque && encoding != PTI_RGBA8) || lm->pixbytes != 4 ||
        !tex || tex->status != TEX_LOADED || tex->gpustream || !img || !img->texture ||
        img->format != GPU_TextureFormat(encoding) || img->type != SDL_GPU_TEXTURETYPE_2D ||
        img->levels != 1 || img->width != lm->width || img->height != lm->height ||
        rect.l >= rect.r || rect.t >= rect.b || rect.r > lm->width || rect.b > lm->height ||
        (!rect.l && !rect.t && rect.r == lm->width && rect.b == lm->height))
        return false;

    *region = (gpu_lightmap_region_t){.lightmap = lm, .opaque = opaque,
        .dst = {.texture = img->texture, .x = rect.l, .y = rect.t,
                .w = rect.r - rect.l, .h = rect.b - rect.t, .d = 1}};
    region->bytes = region->dst.w * region->dst.h * 4;
    return true;
}

static void GPU_ClearLightmapChanges(lightmapinfo_t *lm)
{
    lm->modified = false;
    lm->rectchange.l = lm->width; lm->rectchange.t = lm->height;
    lm->rectchange.r = lm->rectchange.b = 0;
}

static void GPU_UploadFullLightmap(lightmapinfo_t *lm)
{
    struct pendingtextureinfo mips = {0};
    if (!lm->lightmap_texture)
        lm->lightmap_texture = Image_CreateTexture("***lightmap***", NULL, IF_LINEAR | IF_CLAMP | IF_NOMIPMAP);
    mips.type = PTI_2D; mips.encoding = lm->fmt; mips.mipcount = 1;
    if (mips.encoding == PTI_BGRA8) mips.encoding = PTI_BGRX8;
    mips.mip[0].data = lm->lightmaps; mips.mip[0].width = lm->width;
    mips.mip[0].height = lm->height; mips.mip[0].depth = 1;
    mips.mip[0].datasize = lm->width*lm->height*lm->pixbytes;
    if (!GPU_LoadTextureMips(lm->lightmap_texture, &mips))
        return;
    lm->lightmap_texture->status = TEX_LOADED;
    lm->lightmap_texture->width = lm->width; lm->lightmap_texture->height = lm->height;
    gpu.lightmapbytes += mips.mip[0].datasize * (mips.encoding == PTI_L8 ? 4 : 1);
    GPU_ClearLightmapChanges(lm);
}

static qboolean GPU_UploadLightmapRegions(const gpu_lightmap_region_t *regions, unsigned count, Uint32 bytes)
{
    SDL_GPUTransferBufferCreateInfo ti = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = bytes};
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(gpu.device, &ti);
    SDL_GPUCommandBuffer *command;
    SDL_GPUCopyPass *copy;
    qbyte *mapped;

    if (!transfer)
        return false;
    mapped = SDL_MapGPUTransferBuffer(gpu.device, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(gpu.device, transfer);
        return false;
    }
    for (unsigned i = 0; i < count; ++i) {
        const gpu_lightmap_region_t *region = &regions[i];
        const SDL_GPUTextureRegion *dst = &region->dst;
        const lightmapinfo_t *lm = region->lightmap;
        unsigned pitch = dst->w * 4;
        unsigned stride = lm->width * 4;
        qbyte *pixels = mapped + region->offset;
        for (unsigned row = 0; row < dst->h; ++row)
            memcpy(pixels + row * pitch, lm->lightmaps + (dst->y + row) * stride + dst->x * 4, pitch);
        if (region->opaque)
            for (unsigned b = 3; b < region->bytes; b += 4) pixels[b] = 255;
    }
    SDL_UnmapGPUTransferBuffer(gpu.device, transfer);
    command = SDL_AcquireGPUCommandBuffer(gpu.device);
    if (!command) {
        SDL_ReleaseGPUTransferBuffer(gpu.device, transfer);
        return false;
    }
    copy = SDL_BeginGPUCopyPass(command);
    for (unsigned i = 0; i < count; ++i) {
        SDL_GPUTextureTransferInfo src = {.transfer_buffer = transfer, .offset = regions[i].offset};
        // Preserve texels outside each dirty rectangle.
        SDL_UploadToGPUTexture(copy, &src, &regions[i].dst, false);
    }
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(command)) GPU_Error("submit lightmap regions");
    SDL_ReleaseGPUTransferBuffer(gpu.device, transfer);
    return true;
}

static void GPU_FlushLightmapRegions(gpu_lightmap_region_t *regions, unsigned count, Uint32 bytes)
{
    if (!count)
        return;
    if (GPU_UploadLightmapRegions(regions, count, bytes)) {
        for (unsigned i = 0; i < count; ++i) {
            gpu.lightmapbytes += regions[i].bytes;
            GPU_ClearLightmapChanges(regions[i].lightmap);
        }
    } else {
        for (unsigned i = 0; i < count; ++i)
            GPU_UploadFullLightmap(regions[i].lightmap);
    }
}

void GPU_UploadLightmaps(qboolean force)
{
    gpu_lightmap_region_t regions[64];
    unsigned count = 0;
    Uint32 bytes = 0;

    for (unsigned i = 0; i < numlightmaps; ++i) {
        lightmapinfo_t *lm = lightmap[i];
        gpu_lightmap_region_t region;
        if (!lm || lm->external || (!force && !lm->modified)) continue;
        if (!force && gpu_lightmap_partial.ival &&
            GPU_LightmapRegion(lm, lm->fmt == PTI_BGRA8 ? PTI_BGRX8 : lm->fmt, &region)) {
            size_t offset = ((size_t)bytes + 15) & ~(size_t)15;
            if (count == countof(regions) || offset > UINT32_MAX - region.bytes) {
                GPU_FlushLightmapRegions(regions, count, bytes);
                count = 0;
                offset = 0;
            }
            region.offset = offset;
            regions[count++] = region;
            bytes = region.offset + region.bytes;
        } else {
            // Full uploads submit their own commands. Keep atlas upload order.
            GPU_FlushLightmapRegions(regions, count, bytes);
            count = bytes = 0;
            GPU_UploadFullLightmap(lm);
        }
    }
    GPU_FlushLightmapRegions(regions, count, bytes);
}
