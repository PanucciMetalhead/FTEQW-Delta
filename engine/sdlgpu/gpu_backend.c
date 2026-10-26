/* Material batches and command submission for SDL3 GPU. GPL-2.0-or-later. */
#include "gpu_local.h"
#include "renderque.h"
#include "gpu_shaders.h"
#include <stddef.h>
#include <float.h>
#include "panini.h"
#include "gpu_cull.h"

extern cvar_t r_panini_scale, r_panini_filter, r_panini_sharpen;

extern texid_t r_whiteimage, r_blackimage, missing_texture, missing_texture_normal, missing_texture_gloss;
extern cvar_t gl_overbright, gl_overbright_all, gl_specular, gl_specular_power, r_glsl_offsetmapping_scale;
extern cvar_t r_hdr_framebuffer, r_fxaa, r_deluxemapping_cvar;
extern cvar_t v_gamma, v_gammainverted, v_contrast, v_brightness, v_contrastboost, vid_vsync;
static entity_t *entity;
static backendmode_t mode;
static srect_t scissor;
static qboolean hasscissor;
static batch_t **tempbatches;
static size_t numtempbatches, maxtempbatches;
static int batchlightmap = -1;

extern cvar_t r_shadow_realtime_world_shadows, r_shadow_realtime_dlight_shadows;
extern cvar_t r_shadow_shadowmapping, r_shadow_shadowmapping_nearclip, r_shadow_shadowmapping_bias;
extern cvar_t r_shadow_realtime_world_importlightentitiesfrommap;
static struct {
    dlight_t *light;
    vec3_t origin;
    float radius;
    float nearclip;
} shadowlights[GPU_MAX_SHADOWS];
static unsigned numshadowlights;
static qboolean collectshadowonly;

/* Immutable world vertices share pages so adjacent surfaces can still batch.
 * Indices remain in the frame stream: visibility and material order can change.
 * A page fits both 16-bit and 32-bit index builds without rebasing its vertices. */
#define GPU_STATIC_VERTICES 65536
#define GPU_STATIC_BUCKETS 4096
typedef struct gpu_static_page_s {
    SDL_GPUBuffer *buffer;
    gpu_vertex_t *vertices;
    unsigned count, uploaded;
    struct gpu_static_page_s *next;
} gpu_static_page_t;
typedef struct gpu_static_mesh_s {
    const mesh_t *mesh;
    gpu_static_page_t *page;
    unsigned first;
    vec3_t mins, maxs;
    struct gpu_static_mesh_s *next;
} gpu_static_mesh_t;
static gpu_static_page_t *staticpages;
static gpu_static_mesh_t *staticmeshes[GPU_STATIC_BUCKETS];
static model_t *staticworld;
static msurface_t *staticsurfaces;
static qboolean staticinvalid;
static unsigned staticpagecount;
static cvar_t gpu_staticgeometry = CVARD("gpu_staticgeometry", "1", "Keep immutable world vertices on the GPU. Zero uses the reference streaming path.");
static cvar_t gpu_shadowcull = CVARD("gpu_shadowcull", "1", "Cull shadow casters against each light and cubemap face. Zero submits all casters for comparison.");
static cvar_t gpu_bloom_pyramid = CVARD("gpu_bloom_pyramid", "1", "Use a downsampled bloom pyramid. Zero selects the reference full-resolution filter.");

static void GPU_FreeStaticGeometry(void)
{
    unsigned i;
    for (i = 0; i < GPU_STATIC_BUCKETS; ++i) while (staticmeshes[i]) {
        gpu_static_mesh_t *m = staticmeshes[i];
        staticmeshes[i] = m->next; Z_Free(m);
    }
    while (staticpages) {
        gpu_static_page_t *p = staticpages;
        staticpages = p->next;
        SDL_ReleaseGPUBuffer(gpu.device, p->buffer);
        Z_Free(p->vertices); Z_Free(p);
    }
    staticpagecount = 0;
}

static unsigned GPU_StaticHash(const mesh_t *mesh)
{
    return ((uintptr_t)mesh >> 4) % GPU_STATIC_BUCKETS;
}

static gpu_static_mesh_t *GPU_StaticMesh(const mesh_t *mesh)
{
    gpu_static_mesh_t *m;
    for (m = staticmeshes[GPU_StaticHash(mesh)]; m; m = m->next)
        if (m->mesh == mesh) return m;
    return NULL;
}

static gpu_static_mesh_t *GPU_CacheStaticMesh(const mesh_t *mesh, const gpu_vertex_t *vertices)
{
    gpu_static_page_t *p = staticpages;
    gpu_static_mesh_t *m;
    unsigned i, j, hash = GPU_StaticHash(mesh);
    if (!p || p->count + mesh->numvertexes > GPU_STATIC_VERTICES) {
        SDL_GPUBufferCreateInfo bi = {0};
        /* Bound storage even for pathological maps. Further surfaces stream. */
        if (staticpagecount >= 16) return NULL;
        p = Z_Malloc(sizeof(*p));
        bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX; bi.size = GPU_STATIC_VERTICES*sizeof(gpu_vertex_t);
        p->buffer = SDL_CreateGPUBuffer(gpu.device, &bi);
        if (!p->buffer) GPU_Error("create static geometry page");
        p->vertices = BZ_Malloc(bi.size);
        p->next = staticpages; staticpages = p; ++staticpagecount;
    }
    m = Z_Malloc(sizeof(*m)); m->mesh = mesh; m->page = p; m->first = p->count;
    memcpy(p->vertices+p->count, vertices, mesh->numvertexes*sizeof(*vertices));
    for (j = 0; j < 3; ++j) m->mins[j] = m->maxs[j] = vertices[0].position[j];
    for (i = 1; i < mesh->numvertexes; ++i) for (j = 0; j < 3; ++j) {
        m->mins[j] = min(m->mins[j], vertices[i].position[j]);
        m->maxs[j] = max(m->maxs[j], vertices[i].position[j]);
    }
    p->count += mesh->numvertexes;
    m->next = staticmeshes[hash]; staticmeshes[hash] = m;
    return m;
}

void GPU_CheckLightingSettings(void)
{
    if (r_shadow_realtime_world.modified || r_shadow_realtime_world_shadows.modified ||
        r_shadow_realtime_dlight.modified || r_shadow_realtime_dlight_shadows.modified ||
        r_shadow_shadowmapping.modified || r_shadow_realtime_world_importlightentitiesfrommap.modified) {
        Sh_CheckSettings();
        Sh_PreGenerateLights();
        r_shadow_realtime_world.modified = r_shadow_realtime_world_shadows.modified = false;
        r_shadow_realtime_dlight.modified = r_shadow_realtime_dlight_shadows.modified = false;
        r_shadow_shadowmapping.modified = r_shadow_realtime_world_importlightentitiesfrommap.modified = false;
    }
}

typedef struct pipeline_s {
    unsigned bits, cull;
    qboolean lines, panini;
    SDL_GPUTextureFormat format;
    SDL_GPUGraphicsPipeline *pipeline;
    struct pipeline_s *next;
} pipeline_t;
static pipeline_t *pipelines;
typedef struct gpu_target_s {
    texid_t image;
    SDL_GPUTexture *color, *depth;
    unsigned width, height;
    struct gpu_target_s *next;
} gpu_target_t;
static gpu_target_t *targets;

/* One cached source per view, including split-screen and CSQC render targets.
 * A later view must not overwrite an image referenced by an earlier warp. */
struct gpu_panini_view_s {
    SDL_GPUTexture *color, *depth, *destination, *destinationdepth;
    SDL_GPUTextureFormat format;
    unsigned width, height, requestedwidth, requestedheight, destinationwidth, destinationheight;
    pxrect_t rect;
    float warp;
    qboolean used, populated;
    size_t firstdraw;
    struct gpu_panini_view_s *next;
};
static gpu_panini_view_t *paniniviews;

gpu_panini_view_t *GPU_BeginPaniniView(float fovx)
{
    gpu_panini_view_t *view, *available = NULL;
    SDL_GPUTextureCreateInfo ci = {0};
    unsigned basewidth = max(1, r_refdef.pxrect.width), baseheight = max(1, r_refdef.pxrect.height);
    unsigned width, height;
    GPU_PaniniDimensions(basewidth, baseheight, r_panini_scale.value, &width, &height);
    for (view = paniniviews; view; view = view->next) if (!view->used) {
        if (!available) available = view;
        if (view->requestedwidth == width && view->requestedheight == height && view->format == gpu.targetformat) break;
    }
    if (!view) view = available;
    if (!view) {
        view = Z_Malloc(sizeof(*view)); view->next = paniniviews; paniniviews = view;
    }
    view->used = true; view->firstdraw = gpu.numdraws;
    if (!view->color || view->requestedwidth != width || view->requestedheight != height || view->format != gpu.targetformat) {
        if (view->color) SDL_ReleaseGPUTexture(gpu.device, view->color);
        if (view->depth) SDL_ReleaseGPUTexture(gpu.device, view->depth);
        view->requestedwidth = width; view->requestedheight = height;
        ci.type = SDL_GPU_TEXTURETYPE_2D; ci.width = width; ci.height = height;
        ci.layer_count_or_depth = 1; ci.num_levels = GPU_PaniniLevels(width, height);
        ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        ci.format = gpu.targetformat; ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        view->color = SDL_CreateGPUTexture(gpu.device, &ci);
        ci.num_levels = 1; ci.format = gpu.depthformat; ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        view->depth = SDL_CreateGPUTexture(gpu.device, &ci);
        if ((!view->color || !view->depth) && (width != basewidth || height != baseheight)) {
            /* Some devices have smaller limits or insufficient memory. Fall
             * back once per requested size, rather than retrying every frame. */
            Con_Printf("Panini source %ux%u unavailable (%s); using native %ux%u.\n",
                width, height, SDL_GetError(), basewidth, baseheight);
            if (view->color) SDL_ReleaseGPUTexture(gpu.device, view->color);
            if (view->depth) SDL_ReleaseGPUTexture(gpu.device, view->depth);
            width = basewidth; height = baseheight; ci.width = width; ci.height = height;
            ci.num_levels = GPU_PaniniLevels(width, height); ci.format = gpu.targetformat; ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            view->color = SDL_CreateGPUTexture(gpu.device, &ci);
            ci.num_levels = 1; ci.format = gpu.depthformat; ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            view->depth = SDL_CreateGPUTexture(gpu.device, &ci);
        }
        if (!view->color || !view->depth) GPU_Error("create Panini view");
        view->width = width; view->height = height; view->format = gpu.targetformat;
    }
    view->destination = gpu.target; view->destinationdepth = gpu.targetdepth;
    view->destinationwidth = gpu.targetwidth; view->destinationheight = gpu.targetheight;
    view->rect = r_refdef.pxrect;
    /* Standard Panini (D=1): x=2*tan(longitude/2). Screen fitting preserves
     * horizontal coverage and keeps every output sample inside the source.
     * The same factor on both axes preserves proportions at screen center. */
    view->warp = tan(bound(0.001f, fovx, 179.0f) * M_PI / 720.0);
    view->warp *= view->warp;
    gpu.target = view->color; gpu.targetdepth = view->depth;
    gpu.targetwidth = view->width; gpu.targetheight = view->height;
    r_refdef.pxrect.x = r_refdef.pxrect.y = 0;
    r_refdef.pxrect.width = view->width; r_refdef.pxrect.height = view->height;
    return view;
}

void GPU_EndPaniniView(gpu_panini_view_t *view)
{
    gpu_draw_t *draw;
    size_t i;
    view->populated = false;
    for (i = view->firstdraw; i < gpu.numdraws; ++i)
        if (!gpu.draws[i].shadowonly && gpu.draws[i].target == view->color) { view->populated = true; break; }
    /* Even an empty CSQC scene needs a defined source image. */
    if (gpu.numdraws == gpu.maxdraws) {
        gpu.maxdraws = max(256, gpu.maxdraws*2);
        gpu.draws = BZ_Realloc(gpu.draws, gpu.maxdraws*sizeof(*gpu.draws));
    }
    draw = &gpu.draws[gpu.numdraws++]; memset(draw, 0, sizeof(*draw));
    draw->panini = true; draw->projection[0] = view->warp;
    draw->projection[1] = 1.0f / view->width; draw->projection[2] = 1.0f / view->height;
    draw->projection[3] = isfinite(r_panini_sharpen.value) ? bound(0, r_panini_sharpen.value, 1) : 0;
    draw->projection[4] = r_panini_filter.ival == 1;
    draw->projection[5] = GPU_PaniniLevels(view->width, view->height) > 1;
    draw->target = view->destination; draw->depth = view->destinationdepth; draw->format = view->format;
    draw->viewport.x = view->rect.x; draw->viewport.y = view->rect.y;
    draw->viewport.w = view->rect.width; draw->viewport.h = view->rect.height;
    draw->viewport.max_depth = 1;
    draw->scissor.w = view->destinationwidth; draw->scissor.h = view->destinationheight;
    draw->samplers[0].texture = view->color; draw->samplers[0].sampler = gpu.paninisampler;
    gpu.target = view->destination; gpu.targetdepth = view->destinationdepth;
    gpu.targetwidth = view->destinationwidth; gpu.targetheight = view->destinationheight;
    r_refdef.pxrect = view->rect;
}

void GPU_PaniniInfo(void)
{
    gpu_panini_view_t *view;
    for (view = paniniviews; view; view = view->next) if (view->used)
        Con_Printf("Panini view %ux%u: source %ux%u, %s, sharpening %.2f\n",
            view->rect.width, view->rect.height, view->width, view->height,
            r_panini_filter.ival == 1 ? "bicubic" : "bilinear",
            isfinite(r_panini_sharpen.value) ? bound(0, r_panini_sharpen.value, 1) : 0);
}

/* Separate images per visible plane prevent a later water view from replacing
   a texture already referenced by an earlier queued draw. */
#define GPU_MAX_WATER_VIEWS 4
typedef struct {
    vec4_t plane;
    SDL_GPUTexture *color[2], *depth[2];
    SDL_GPUTextureFormat format;
    unsigned width, height, flags;
    unsigned draws[2];
    float screenmatrix[16];
} gpu_water_view_t;
static gpu_water_view_t waterviews[GPU_MAX_WATER_VIEWS];
static unsigned numwaterviews;
static gpu_water_view_t *currentwater;
static vec4_t clipplane;
static qboolean clipenabled;
void GPU_WaterInfo(void)
{
    unsigned i;
    for (i = 0; i < numwaterviews; ++i) {
        gpu_water_view_t *v = &waterviews[i];
        Con_Printf("Water view %u: plane %.3f %.3f %.3f %.3f; %ux%u; reflection/refraction %u/%u draws\n",
            i,v->plane[0],v->plane[1],v->plane[2],v->plane[3],v->width,v->height,v->draws[0],v->draws[1]);
    }
}

static void GPU_FreeWaterViews(void)
{
    unsigned i, j;
    for (i = 0; i < GPU_MAX_WATER_VIEWS; ++i) for (j = 0; j < 2; ++j) {
        if (waterviews[i].color[j]) SDL_ReleaseGPUTexture(gpu.device, waterviews[i].color[j]);
        if (waterviews[i].depth[j]) SDL_ReleaseGPUTexture(gpu.device, waterviews[i].depth[j]);
    }
    memset(waterviews, 0, sizeof(waterviews));
    numwaterviews = 0; currentwater = NULL;
}

static SDL_GPUShader *GPU_Shader(const unsigned char *spirv, size_t spirvsize, const unsigned char *msl,
    SDL_GPUShaderStage stage, unsigned samplers, unsigned uniforms, unsigned storages)
{
    SDL_GPUShaderCreateInfo info = {0};
    if (SDL_GetGPUShaderFormats(gpu.device) & SDL_GPU_SHADERFORMAT_MSL) {
        info.code = msl; info.code_size = strlen((const char *)msl);
        info.format = SDL_GPU_SHADERFORMAT_MSL; info.entrypoint = "main0";
    } else {
        info.code = spirv; info.code_size = spirvsize;
        info.format = SDL_GPU_SHADERFORMAT_SPIRV; info.entrypoint = "main";
    }
    info.stage = stage; info.num_samplers = samplers; info.num_uniform_buffers = uniforms; info.num_storage_buffers = storages;
    SDL_GPUShader *shader = SDL_CreateGPUShader(gpu.device, &info);
    if (!shader) GPU_Error("create shader");
    return shader;
}

static SDL_GPUBlendFactor GPU_BlendFactor(unsigned bits)
{
    static const SDL_GPUBlendFactor factors[] = {
        SDL_GPU_BLENDFACTOR_ONE, SDL_GPU_BLENDFACTOR_ZERO, SDL_GPU_BLENDFACTOR_ONE,
        SDL_GPU_BLENDFACTOR_DST_COLOR, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR,
        SDL_GPU_BLENDFACTOR_SRC_ALPHA, SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_GPU_BLENDFACTOR_DST_ALPHA, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
        SDL_GPU_BLENDFACTOR_SRC_COLOR, SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
        SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE
    };
    return bits < countof(factors) ? factors[bits] : SDL_GPU_BLENDFACTOR_ONE;
}

static SDL_GPUGraphicsPipeline *GPU_Pipeline(unsigned bits, unsigned cull, qboolean lines, SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    SDL_GPUColorTargetDescription color = {0};
    SDL_GPUVertexBufferDescription buffer = {0};
    SDL_GPUVertexAttribute attrs[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(gpu_vertex_t, position)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(gpu_vertex_t, uv)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(gpu_vertex_t, lmuv)},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(gpu_vertex_t, color)},
        {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(gpu_vertex_t, normal)},
        {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(gpu_vertex_t, tangent)},
        {6, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(gpu_vertex_t, bitangent)}
    };
    pipeline_t *p;
    /* Alpha test is a fragment uniform, so it needn't multiply pipeline variants. */
    bits &= ~SBITS_ATEST_BITS;
    for (p = pipelines; p; p = p->next)
        if (!p->panini && p->bits == bits && p->cull == cull && p->lines == lines && p->format == format) return p->pipeline;
    buffer.pitch = sizeof(gpu_vertex_t); buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    info.vertex_shader = gpu.vertexshader; info.fragment_shader = gpu.fragmentshader;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = countof(attrs);
    info.primitive_type = lines ? SDL_GPU_PRIMITIVETYPE_LINELIST : SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.cull_mode = (cull & SHADER_CULL_FRONT) ? SDL_GPU_CULLMODE_FRONT :
        (cull & SHADER_CULL_BACK) ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.enable_depth_test = !(bits & SBITS_MISC_NODEPTHTEST);
    info.depth_stencil_state.enable_depth_write = !!(bits & SBITS_MISC_DEPTHWRITE);
    switch (bits & SBITS_DEPTHFUNC_BITS) {
    case SBITS_DEPTHFUNC_EQUAL: info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL; break;
    case SBITS_DEPTHFUNC_CLOSER: info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS; break;
    case SBITS_DEPTHFUNC_FURTHER: info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER; break;
    default: info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    }
    color.format = format;
    color.blend_state.enable_blend = !!(bits & SBITS_BLEND_BITS);
    color.blend_state.src_color_blendfactor = GPU_BlendFactor(bits & SBITS_SRCBLEND_BITS);
    color.blend_state.dst_color_blendfactor = GPU_BlendFactor((bits & SBITS_DSTBLEND_BITS) >> 4);
    color.blend_state.color_blend_op = color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.enable_color_write_mask = true;
    color.blend_state.color_write_mask =
        ((bits & SBITS_MASK_RED) ? 0 : SDL_GPU_COLORCOMPONENT_R) |
        ((bits & SBITS_MASK_GREEN) ? 0 : SDL_GPU_COLORCOMPONENT_G) |
        ((bits & SBITS_MASK_BLUE) ? 0 : SDL_GPU_COLORCOMPONENT_B) |
        ((bits & SBITS_MASK_ALPHA) ? 0 : SDL_GPU_COLORCOMPONENT_A);
    info.target_info.color_target_descriptions = &color; info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = gpu.depthformat;
    p = Z_Malloc(sizeof(*p)); p->bits = bits; p->cull = cull; p->lines = lines; p->format = format;
    p->pipeline = SDL_CreateGPUGraphicsPipeline(gpu.device, &info);
    if (!p->pipeline) GPU_Error("create material pipeline");
    p->next = pipelines; pipelines = p;
    return p->pipeline;
}

static SDL_GPUGraphicsPipeline *GPU_PaniniPipeline(SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    SDL_GPUColorTargetDescription color = {0};
    pipeline_t *p;
    for (p = pipelines; p; p = p->next) if (p->panini && p->format == format) return p->pipeline;
    info.vertex_shader = gpu.postvertex; info.fragment_shader = gpu.paninifragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    color.format = format;
    info.target_info.color_target_descriptions = &color; info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true; info.target_info.depth_stencil_format = gpu.depthformat;
    p = Z_Malloc(sizeof(*p)); p->panini = true; p->format = format;
    p->pipeline = SDL_CreateGPUGraphicsPipeline(gpu.device, &info);
    if (!p->pipeline) GPU_Error("create Panini pipeline");
    p->next = pipelines; pipelines = p;
    return p->pipeline;
}

void GPU_BackendInit(void)
{
    SDL_GPUGraphicsPipelineCreateInfo pi = {0};
    SDL_GPUColorTargetDescription color = {0};
    SDL_GPUSamplerCreateInfo si = {0};
    if (Cvar_FindVar(gpu_staticgeometry.name) != &gpu_staticgeometry) Cvar_Register(&gpu_staticgeometry, "SDL GPU");
    if (Cvar_FindVar(gpu_shadowcull.name) != &gpu_shadowcull) Cvar_Register(&gpu_shadowcull, "SDL GPU");
    if (Cvar_FindVar(gpu_bloom_pyramid.name) != &gpu_bloom_pyramid) Cvar_Register(&gpu_bloom_pyramid, "SDL GPU");
    gpu.vertexshader = GPU_Shader(gpu_scene_vert_spv, sizeof(gpu_scene_vert_spv), gpu_scene_vert_msl, SDL_GPU_SHADERSTAGE_VERTEX, 0, 1, 0);
    gpu.fragmentshader = GPU_Shader(gpu_scene_frag_spv, sizeof(gpu_scene_frag_spv), gpu_scene_frag_msl, SDL_GPU_SHADERSTAGE_FRAGMENT, GPU_SAMPLERS, 1, 3);
    gpu.postvertex = GPU_Shader(gpu_post_vert_spv, sizeof(gpu_post_vert_spv), gpu_post_vert_msl, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0);
    gpu.postfragment = GPU_Shader(gpu_post_frag_spv, sizeof(gpu_post_frag_spv), gpu_post_frag_msl, SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 1, 0);
    gpu.bloomshader = GPU_Shader(gpu_bloom_frag_spv, sizeof(gpu_bloom_frag_spv), gpu_bloom_frag_msl, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 0);
    gpu.paninifragment = GPU_Shader(gpu_panini_frag_spv, sizeof(gpu_panini_frag_spv), gpu_panini_frag_msl, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 0);
    pi.vertex_shader = gpu.postvertex; pi.fragment_shader = gpu.postfragment;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    color.format = gpu.colorformat;
    pi.target_info.color_target_descriptions = &color; pi.target_info.num_color_targets = 1;
    gpu.postpipeline = SDL_CreateGPUGraphicsPipeline(gpu.device, &pi);
    if (!gpu.postpipeline) GPU_Error("create presentation pipeline");
    si.min_filter = si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    gpu.postsampler = SDL_CreateGPUSampler(gpu.device, &si);
    if (!gpu.postsampler) GPU_Error("create presentation sampler");
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR; si.max_lod = 32;
    si.enable_anisotropy = true; si.max_anisotropy = 4;
    gpu.paninisampler = SDL_CreateGPUSampler(gpu.device, &si);
    if (!gpu.paninisampler) GPU_Error("create Panini sampler");
    {
        /* Every draw must bind a cube-compatible texture, including UI and
         * classic skies which do not sample this slot. Clear all six faces. */
        SDL_GPUTextureCreateInfo ci = {0};
        SDL_GPUCommandBuffer *command;
        unsigned face;
        ci.type = SDL_GPU_TEXTURETYPE_CUBE; ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ci.width = ci.height = ci.num_levels = 1; ci.layer_count_or_depth = 6;
        ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        gpu.fallbackcube = SDL_CreateGPUTexture(gpu.device, &ci);
        if (!gpu.fallbackcube) GPU_Error("create fallback cube");
        command = SDL_AcquireGPUCommandBuffer(gpu.device);
        if (!command) GPU_Error("acquire fallback cube commands");
        for (face = 0; face < 6; ++face) {
            SDL_GPUColorTargetInfo target = {0};
            SDL_GPURenderPass *pass;
            target.texture = gpu.fallbackcube; target.layer_or_depth_plane = face;
            target.load_op = SDL_GPU_LOADOP_CLEAR; target.store_op = SDL_GPU_STOREOP_STORE;
            target.clear_color.a = 1;
            pass = SDL_BeginGPURenderPass(command, &target, 1, NULL);
            SDL_EndGPURenderPass(pass);
        }
        if (!SDL_SubmitGPUCommandBuffer(command)) GPU_Error("clear fallback cube");
    }
    gpu.shadowvertex = GPU_Shader(gpu_shadow_vert_spv, sizeof(gpu_shadow_vert_spv), gpu_shadow_vert_msl, SDL_GPU_SHADERSTAGE_VERTEX, 0, 1, 0);
    gpu.shadowfragment = GPU_Shader(gpu_shadow_frag_spv, sizeof(gpu_shadow_frag_spv), gpu_shadow_frag_msl, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 0);
    {
        SDL_GPUVertexBufferDescription buffer = {0};
        SDL_GPUVertexAttribute attrs[] = {
            {0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(gpu_vertex_t,position)},
            {1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(gpu_vertex_t,uv)}
        };
        SDL_GPUTextureCreateInfo ci = {0};
        memset(&pi, 0, sizeof(pi));
        pi.vertex_shader = gpu.shadowvertex; pi.fragment_shader = gpu.shadowfragment;
        buffer.pitch = sizeof(gpu_vertex_t); buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        pi.vertex_input_state.vertex_buffer_descriptions = &buffer;
        pi.vertex_input_state.num_vertex_buffers = 1;
        pi.vertex_input_state.vertex_attributes = attrs; pi.vertex_input_state.num_vertex_attributes = countof(attrs);
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        pi.depth_stencil_state.enable_depth_test = pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.target_info.has_depth_stencil_target = true; pi.target_info.depth_stencil_format = gpu.depthformat;
        gpu.shadowpipeline = SDL_CreateGPUGraphicsPipeline(gpu.device, &pi);
        if (!gpu.shadowpipeline) GPU_Error("create shadow pipeline");
        ci.type = SDL_GPU_TEXTURETYPE_CUBE_ARRAY; ci.format = gpu.depthformat;
        ci.width = ci.height = GPU_SHADOW_SIZE; ci.layer_count_or_depth = GPU_MAX_SHADOWS*6;
        ci.num_levels = 1; ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        gpu.shadowmap = SDL_CreateGPUTexture(gpu.device, &ci);
        if (!gpu.shadowmap) GPU_Error("create shadow cubemaps");
        memset(&si, 0, sizeof(si)); si.min_filter = si.mag_filter = SDL_GPU_FILTER_LINEAR;
        si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.enable_compare = true; si.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        gpu.shadowsampler = SDL_CreateGPUSampler(gpu.device, &si);
        if (!gpu.shadowsampler) GPU_Error("create shadow sampler");
    }
    entity = &r_worldentity; mode = BEM_STANDARD;
    gpu.batching = true;
    GPU_LightingInit();
    be_maxpasses = 1;
}

void GPU_BackendShutdown(void)
{
    pipeline_t *p;
    size_t i;
    GPU_FreeStaticGeometry(); staticworld = NULL; staticsurfaces = NULL; staticinvalid = false;
    GPU_LightingShutdown();
    for (i = 0; i < GPU_BLOOM_LEVELS; ++i)
        if (gpu.bloom[i]) SDL_ReleaseGPUTexture(gpu.device, gpu.bloom[i]);
    for (i = 0; i < 2; ++i)
        if (gpu.bloompipeline[i]) SDL_ReleaseGPUGraphicsPipeline(gpu.device, gpu.bloompipeline[i]);
    if (gpu.bloomshader) SDL_ReleaseGPUShader(gpu.device, gpu.bloomshader);
    GPU_FreeWaterViews();
    while (paniniviews) {
        gpu_panini_view_t *view = paniniviews;
        paniniviews = view->next;
        SDL_ReleaseGPUTexture(gpu.device, view->color); SDL_ReleaseGPUTexture(gpu.device, view->depth);
        Z_Free(view);
    }
    if (gpu.paninifragment) SDL_ReleaseGPUShader(gpu.device, gpu.paninifragment);
    while ((p = pipelines)) {
        pipelines = p->next; SDL_ReleaseGPUGraphicsPipeline(gpu.device, p->pipeline); Z_Free(p);
    }
    while (targets) {
        gpu_target_t *t = targets;
        targets = t->next;
        SDL_ReleaseGPUTexture(gpu.device, t->depth); Z_Free(t);
    }
    if (gpu.shadowpipeline) SDL_ReleaseGPUGraphicsPipeline(gpu.device, gpu.shadowpipeline);
    if (gpu.shadowvertex) SDL_ReleaseGPUShader(gpu.device, gpu.shadowvertex);
    if (gpu.shadowfragment) SDL_ReleaseGPUShader(gpu.device, gpu.shadowfragment);
    if (gpu.shadowmap) SDL_ReleaseGPUTexture(gpu.device, gpu.shadowmap);
    if (gpu.fallbackcube) SDL_ReleaseGPUTexture(gpu.device, gpu.fallbackcube);
    if (gpu.shadowsampler) SDL_ReleaseGPUSampler(gpu.device, gpu.shadowsampler);
    if (gpu.present) SDL_ReleaseGPUTexture(gpu.device, gpu.present);
    if (gpu.postpipeline) SDL_ReleaseGPUGraphicsPipeline(gpu.device, gpu.postpipeline);
    if (gpu.vertexshader) SDL_ReleaseGPUShader(gpu.device, gpu.vertexshader);
    if (gpu.fragmentshader) SDL_ReleaseGPUShader(gpu.device, gpu.fragmentshader);
    if (gpu.postvertex) SDL_ReleaseGPUShader(gpu.device, gpu.postvertex);
    if (gpu.postfragment) SDL_ReleaseGPUShader(gpu.device, gpu.postfragment);
    if (gpu.postsampler) SDL_ReleaseGPUSampler(gpu.device, gpu.postsampler);
    if (gpu.paninisampler) SDL_ReleaseGPUSampler(gpu.device, gpu.paninisampler);
    if (gpu.vertices) SDL_ReleaseGPUBuffer(gpu.device, gpu.vertices);
    if (gpu.indices) SDL_ReleaseGPUBuffer(gpu.device, gpu.indices);
    if (gpu.transfer) SDL_ReleaseGPUTransferBuffer(gpu.device, gpu.transfer);
    if (gpu.scene) SDL_ReleaseGPUTexture(gpu.device, gpu.scene);
    if (gpu.depth) SDL_ReleaseGPUTexture(gpu.device, gpu.depth);
    Z_Free(gpu.cpuvertices); Z_Free(gpu.cpuindices); Z_Free(gpu.draws);
    for (i = 0; i < maxtempbatches; ++i) Z_Free(tempbatches[i]);
    Z_Free(tempbatches); tempbatches = NULL; numtempbatches = maxtempbatches = 0;
}

void GPU_BeginFrame(void)
{
    unsigned w, h;
    if (gpu.command) return;
    if (gpu.benchmarking) gpu.framebegin = Sys_DoubleTime();
    if (staticinvalid || staticworld != cl.worldmodel ||
        (cl.worldmodel && staticsurfaces != cl.worldmodel->surfaces)) {
        GPU_FreeStaticGeometry(); staticworld = cl.worldmodel;
        staticsurfaces = cl.worldmodel ? cl.worldmodel->surfaces : NULL; staticinvalid = false;
    }
    gpu.numvertices = gpu.numstreamvertices = gpu.numindices = gpu.numdraws = numtempbatches = numshadowlights = 0;
    gpu.cachedvertices = gpu.uploadedbytes = gpu.shadowdraws = gpu.shadowculled = 0;
    GPU_LightingBeginFrame();
    collectshadowonly = false;
    gpu.sceneonlygamma = !!vid_sceneonlygamma.ival;
    for (gpu_panini_view_t *view = paniniviews; view; view = view->next) view->used = false;
    numwaterviews = 0; currentwater = NULL; clipenabled = false;
    if (vid_vsync.modified) {
        SDL_GPUPresentMode present = vid_vsync.ival ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
        if (!SDL_WindowSupportsGPUPresentMode(gpu.device, sdlwindow, present)) present = SDL_GPU_PRESENTMODE_VSYNC;
        if (!SDL_SetGPUSwapchainParameters(gpu.device, sdlwindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present)) GPU_Error("set vsync");
        vid_vsync.modified = false;
    }
    gpu.command = SDL_AcquireGPUCommandBuffer(gpu.device);
    if (!gpu.command) GPU_Error("acquire command buffer");
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(gpu.command, sdlwindow, &gpu.swapchain, &w, &h)) GPU_Error("acquire swapchain");
    if (!gpu.swapchain) {
        SDL_CancelGPUCommandBuffer(gpu.command); gpu.command = NULL; return;
    }
    if (gpu.benchmarking) gpu.frameacquired = Sys_DoubleTime();
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    if (r_hdr_framebuffer.ival && SDL_GPUTextureSupportsFormat(gpu.device,
        SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTURETYPE_2D,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER))
        format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    if (format == SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT) vid.flags |= VID_FP16;
    else vid.flags &= ~VID_FP16;
    if (w != gpu.width || h != gpu.height || format != gpu.sceneformat) {
        SDL_GPUTextureCreateInfo ci = {0};
        if (gpu.present) SDL_ReleaseGPUTexture(gpu.device, gpu.present);
        if (gpu.scene) SDL_ReleaseGPUTexture(gpu.device, gpu.scene);
        if (gpu.depth) SDL_ReleaseGPUTexture(gpu.device, gpu.depth);
        gpu.width = vid.pixelwidth = w; gpu.height = vid.pixelheight = h;
        ci.type = SDL_GPU_TEXTURETYPE_2D; ci.width = w; ci.height = h; ci.layer_count_or_depth = 1;
        ci.num_levels = 1; ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        ci.format = gpu.sceneformat = format;
        ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        gpu.scene = SDL_CreateGPUTexture(gpu.device, &ci);
        ci.format = gpu.colorformat;
        gpu.present = SDL_CreateGPUTexture(gpu.device, &ci);
        ci.format = gpu.depthformat; ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        gpu.depth = SDL_CreateGPUTexture(gpu.device, &ci);
        if (!gpu.scene || !gpu.depth || !gpu.present) GPU_Error("resize frame targets");
        R2D_Console_Resize();
    }
    gpu.target = gpu.scene; gpu.targetdepth = gpu.depth; gpu.targetformat = gpu.sceneformat;
    gpu.targetwidth = gpu.width; gpu.targetheight = gpu.height;
}

static void GPU_UploadGeometry(void)
{
    size_t vbytes = gpu.numstreamvertices*sizeof(gpu_vertex_t), ibytes = gpu.numindices*sizeof(index_t);
    size_t total = vbytes + ibytes, offset;
    gpu_static_page_t *p;
    SDL_GPUBufferCreateInfo bi = {0};
    SDL_GPUTransferBufferCreateInfo ti = {0};
    SDL_GPUTransferBufferLocation src = {0};
    SDL_GPUBufferRegion dst = {0};
    SDL_GPUCopyPass *copy;
    qbyte *map;
    for (p = staticpages; p; p = p->next) total += (p->count-p->uploaded)*sizeof(gpu_vertex_t);
    if (!total) return;
    if (vbytes > gpu.vertexcapacity || !gpu.vertices) {
        if (gpu.vertices) SDL_ReleaseGPUBuffer(gpu.device, gpu.vertices);
        bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX; bi.size = gpu.vertexcapacity = max(sizeof(gpu_vertex_t), max(vbytes, gpu.vertexcapacity*2));
        gpu.vertices = SDL_CreateGPUBuffer(gpu.device, &bi);
    }
    if (ibytes > gpu.indexcapacity) {
        if (gpu.indices) SDL_ReleaseGPUBuffer(gpu.device, gpu.indices);
        bi.usage = SDL_GPU_BUFFERUSAGE_INDEX; bi.size = gpu.indexcapacity = max(ibytes, gpu.indexcapacity*2);
        gpu.indices = SDL_CreateGPUBuffer(gpu.device, &bi);
    }
    if (total > gpu.transfercapacity) {
        if (gpu.transfer) SDL_ReleaseGPUTransferBuffer(gpu.device, gpu.transfer);
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; ti.size = gpu.transfercapacity = max(total, gpu.transfercapacity*2);
        gpu.transfer = SDL_CreateGPUTransferBuffer(gpu.device, &ti);
    }
    if (!gpu.vertices || !gpu.indices || !gpu.transfer) GPU_Error("allocate frame geometry");
    map = SDL_MapGPUTransferBuffer(gpu.device, gpu.transfer, true);
    if (!map) GPU_Error("map frame geometry");
    memcpy(map, gpu.cpuvertices, vbytes); memcpy(map+vbytes, gpu.cpuindices, ibytes);
    offset = vbytes+ibytes;
    for (p = staticpages; p; p = p->next) if (p->count > p->uploaded) {
        size_t bytes = (p->count-p->uploaded)*sizeof(gpu_vertex_t);
        memcpy(map+offset, p->vertices+p->uploaded, bytes); offset += bytes;
    }
    SDL_UnmapGPUTransferBuffer(gpu.device, gpu.transfer);
    copy = SDL_BeginGPUCopyPass(gpu.command);
    src.transfer_buffer = gpu.transfer; dst.buffer = gpu.vertices; dst.size = vbytes;
    if (vbytes) SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    src.offset = vbytes; dst.buffer = gpu.indices; dst.size = ibytes;
    if (ibytes) SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    offset = vbytes+ibytes;
    for (p = staticpages; p; p = p->next) if (p->count > p->uploaded) {
        src.offset = offset; dst.buffer = p->buffer;
        dst.offset = p->uploaded*sizeof(gpu_vertex_t); dst.size = (p->count-p->uploaded)*sizeof(gpu_vertex_t);
        /* Append only to previously unreferenced regions. Cycling would discard
         * the immutable vertices referenced by earlier frames. */
        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
        offset += dst.size; p->uploaded = p->count;
    }
    gpu.uploadedbytes = total;
    SDL_EndGPUCopyPass(copy);
}

static void GPU_DrawShadows(void)
{
    static const vec3_t direction[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    static const vec3_t up[6] = {{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
    SDL_GPUBufferBinding vb = {gpu.vertices,0}, ib = {gpu.indices,0};
    unsigned l, face;
    if (!numshadowlights && gpu.shadowinitialized) return;
    /* Clear even unused maps on the first frame: all sampled resources are initialized. */
    for (l = 0; l < max(1, numshadowlights); ++l) for (face = 0; face < 6; ++face) {
        SDL_GPUDepthStencilTargetInfo depth = {0};
        SDL_GPURenderPass *pass;
        float view[16], proj[16], vp[16];
        vec3_t right;
        size_t i;
        depth.texture = gpu.shadowmap; depth.layer = l*6+face;
        depth.clear_depth = 1; depth.load_op = SDL_GPU_LOADOP_CLEAR; depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE; depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        pass = SDL_BeginGPURenderPass(gpu.command, NULL, 0, &depth);
        if (!pass) GPU_Error("begin shadow pass");
        if (l < numshadowlights && gpu.numdraws) {
            CrossProduct(direction[face], up[face], right);
            Matrix4x4_CM_ModelViewMatrixFromAxis(view, direction[face], right, up[face], shadowlights[l].origin);
            Matrix4x4_CM_Projection_Far(proj, 90, 90, shadowlights[l].nearclip, shadowlights[l].radius, true);
            proj[5] = -proj[5]; /* Cube image rows run downwards. */
            Matrix4_Multiply(proj, view, vp);
            SDL_BindGPUGraphicsPipeline(pass, gpu.shadowpipeline);
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
            SDL_BindGPUIndexBuffer(pass, &ib, sizeof(index_t)==2 ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);
            for (i = 0; i < gpu.numdraws; ++i) {
                gpu_draw_t *d = &gpu.draws[i];
                float mvp[16], control[4] = {d->frag.material[2],d->frag.upper[3],0,0};
                if (!d->castshadow || d->lines) continue;
                if (gpu_shadowcull.ival && d->boundsvalid &&
                    !GPU_ShadowBoundsVisible(d->mins, d->maxs, shadowlights[l].origin, shadowlights[l].radius, vp)) {
                    ++gpu.shadowculled; continue;
                }
                if (vb.buffer != (d->vertexbuffer ? d->vertexbuffer : gpu.vertices)) {
                    vb.buffer = d->vertexbuffer ? d->vertexbuffer : gpu.vertices;
                    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
                }
                Matrix4_Multiply(vp, d->vert.model, mvp);
                SDL_PushGPUVertexUniformData(gpu.command, 0, mvp, sizeof(mvp));
                SDL_PushGPUFragmentUniformData(gpu.command, 0, control, sizeof(control));
                SDL_BindGPUFragmentSamplers(pass, 0, &d->samplers[0], 1);
                SDL_DrawGPUIndexedPrimitives(pass, d->numindices, 1, d->firstindex, d->vertexoffset, 0);
                ++gpu.shadowdraws;
            }
        }
        SDL_EndGPURenderPass(pass);
    }
    gpu.shadowinitialized = true;
}

typedef struct gpu_overlay_snapshot_s {
    SDL_GPUTexture *texture;
    struct gpu_overlay_snapshot_s *next;
} gpu_overlay_snapshot_t;

static void GPU_SnapshotOverlay(size_t index, gpu_overlay_snapshot_t **snapshots)
{
    gpu_draw_t *draw = &gpu.draws[index];
    unsigned slot;
    for (slot = 0; slot < GPU_SAMPLERS; ++slot) {
        SDL_GPUTexture *source = draw->samplers[slot].texture;
        gpu_target_t *target;
        gpu_overlay_snapshot_t *snapshot;
        SDL_GPUTextureCreateInfo ci = {0};
        SDL_GPUBlitInfo blit = {0};
        size_t later;
        unsigned other;
        /* Deferred HUD draws must see the RTT contents from their original
         * place in the command stream, not a later repaint of that texture. */
        for (target = targets; target; target = target->next)
            if (target->color == source) break;
        if (!target) continue;
        for (later = index+1; later < gpu.numdraws; ++later)
            if (!gpu.draws[later].shadowonly && gpu.draws[later].target == source) break;
        if (later == gpu.numdraws) continue;
        ci.type = SDL_GPU_TEXTURETYPE_2D; ci.format = target->image->gpuimage->format;
        ci.width = target->width; ci.height = target->height;
        ci.layer_count_or_depth = ci.num_levels = 1;
        ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        snapshot = Z_Malloc(sizeof(*snapshot));
        snapshot->texture = SDL_CreateGPUTexture(gpu.device, &ci);
        if (!snapshot->texture) GPU_Error("create overlay texture snapshot");
        snapshot->next = *snapshots; *snapshots = snapshot;
        blit.source.texture = source; blit.source.w = target->width; blit.source.h = target->height;
        blit.destination.texture = snapshot->texture;
        blit.destination.w = target->width; blit.destination.h = target->height;
        blit.load_op = SDL_GPU_LOADOP_DONT_CARE; blit.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(gpu.command, &blit);
        for (other = slot; other < GPU_SAMPLERS; ++other)
            if (draw->samplers[other].texture == source) draw->samplers[other].texture = snapshot->texture;
    }
}

static void GPU_DrawCommands(SDL_GPURenderPass *pass, size_t first, size_t end, qboolean overlays)
{
    SDL_GPUBufferBinding vb = {gpu.vertices,0}, ib = {gpu.indices,0};
    SDL_GPUGraphicsPipeline *boundpipeline = NULL;
    gpu_draw_t *previous = NULL;
    size_t i;
    if (gpu.vertices && gpu.indices) {
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_BindGPUIndexBuffer(pass, &ib, sizeof(index_t) == 2 ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);
    }
    for (i = first; i < end; ++i) {
        gpu_draw_t *d = &gpu.draws[i];
        SDL_GPUGraphicsPipeline *pipeline;
        if (d->shadowonly || (gpu.sceneonlygamma && d->screenoverlay) != overlays) continue;
        if (d->panini) {
            SDL_BindGPUGraphicsPipeline(pass, GPU_PaniniPipeline(d->format));
            SDL_SetGPUViewport(pass, &d->viewport); SDL_SetGPUScissor(pass, &d->scissor);
            SDL_PushGPUFragmentUniformData(gpu.command, 0, d->projection, sizeof(d->projection));
            SDL_BindGPUFragmentSamplers(pass, 0, d->samplers, 1);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            previous = NULL; boundpipeline = NULL;
            continue;
        }
        /* Overlay pipelines target the corrected SDR image, including when
         * world rendering used an HDR scene buffer. Blend after correction so
         * translucent UI retains its source colors and alpha. */
        pipeline = GPU_Pipeline(d->bits, d->cull, d->lines, overlays ? gpu.colorformat : d->format);
        if (vb.buffer != (d->vertexbuffer ? d->vertexbuffer : gpu.vertices)) {
            vb.buffer = d->vertexbuffer ? d->vertexbuffer : gpu.vertices;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        }
        if (pipeline != boundpipeline) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            boundpipeline = pipeline;
        }
        if (!previous || memcmp(&previous->viewport,&d->viewport,sizeof(d->viewport))) SDL_SetGPUViewport(pass, &d->viewport);
        if (!previous || memcmp(&previous->scissor,&d->scissor,sizeof(d->scissor))) SDL_SetGPUScissor(pass, &d->scissor);
        if (!previous || memcmp(&previous->vert,&d->vert,sizeof(d->vert))) SDL_PushGPUVertexUniformData(gpu.command, 0, &d->vert, sizeof(d->vert));
        if (!previous || memcmp(&previous->frag,&d->frag,sizeof(d->frag))) SDL_PushGPUFragmentUniformData(gpu.command, 0, &d->frag, sizeof(d->frag));
        if (!previous || memcmp(previous->samplers,d->samplers,sizeof(d->samplers))) SDL_BindGPUFragmentSamplers(pass, 0, d->samplers, GPU_SAMPLERS);
        if (!previous || previous->lighting != d->lighting) GPU_LightingBind(pass,d);
        SDL_DrawGPUIndexedPrimitives(pass, d->numindices, 1, d->firstindex, d->vertexoffset, 0);
        previous = gpu.batching ? d : NULL;
    }
}

static SDL_GPUTexture *GPU_Bloom(void)
{
    SDL_GPUTexture *source = gpu.scene;
    unsigned i, width = gpu.width, height = gpu.height;
    if (gpu.bloomwidth != gpu.width || gpu.bloomheight != gpu.height || gpu.bloomformat != gpu.sceneformat) {
        SDL_GPUTextureCreateInfo ci = {0};
        for (i = 0; i < GPU_BLOOM_LEVELS; ++i) {
            if (gpu.bloom[i]) SDL_ReleaseGPUTexture(gpu.device, gpu.bloom[i]);
            gpu.bloom[i] = NULL;
        }
        if (gpu.bloomformat != gpu.sceneformat) {
            for (i = 0; i < 2; ++i) {
                SDL_GPUGraphicsPipelineCreateInfo pi = {0};
                SDL_GPUColorTargetDescription color = {0};
                if (gpu.bloompipeline[i]) SDL_ReleaseGPUGraphicsPipeline(gpu.device, gpu.bloompipeline[i]);
                pi.vertex_shader = gpu.postvertex; pi.fragment_shader = gpu.bloomshader;
                pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pi.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
                color.format = gpu.sceneformat;
                color.blend_state.enable_blend = i != 0;
                color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
                color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
                color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
                pi.target_info.color_target_descriptions = &color; pi.target_info.num_color_targets = 1;
                gpu.bloompipeline[i] = SDL_CreateGPUGraphicsPipeline(gpu.device, &pi);
                if (!gpu.bloompipeline[i]) GPU_Error("create bloom pipeline");
            }
        }
        gpu.bloomwidth = gpu.width; gpu.bloomheight = gpu.height; gpu.bloomformat = gpu.sceneformat;
        ci.type = SDL_GPU_TEXTURETYPE_2D; ci.format = gpu.sceneformat;
        ci.layer_count_or_depth = ci.num_levels = 1; ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        gpu.bloomlevels = 0;
        do {
            width = max(1,(width+1)/2); height = max(1,(height+1)/2);
            ci.width = width; ci.height = height;
            gpu.bloom[gpu.bloomlevels] = SDL_CreateGPUTexture(gpu.device, &ci);
            if (!gpu.bloom[gpu.bloomlevels]) GPU_Error("create bloom pyramid");
            ++gpu.bloomlevels;
        } while (gpu.bloomlevels < GPU_BLOOM_LEVELS && (width > 1 || height > 1));
    }
    width = gpu.width; height = gpu.height;
    /* Downsample then blend each coarser level into its finer neighbour. The
     * half/half upsample keeps a uniform bright field's energy independent of
     * the number of levels. Never sample the texture being rendered to. */
    for (i = 0; i < gpu.bloomlevels*2-1; ++i) {
        qboolean up = i >= gpu.bloomlevels;
        unsigned level = up ? gpu.bloomlevels*2-2-i : i;
        SDL_GPUColorTargetInfo color = {0};
        SDL_GPUTextureSamplerBinding sampler = {source,gpu.postsampler};
        float control[4] = {1.0f/width,1.0f/height,up ? 2 : i ? 1 : 0,0};
        SDL_GPURenderPass *pass;
        color.texture = gpu.bloom[level]; color.load_op = up ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_DONT_CARE;
        color.store_op = SDL_GPU_STOREOP_STORE;
        pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, NULL);
        if (!pass) GPU_Error("begin bloom pass");
        SDL_BindGPUGraphicsPipeline(pass, gpu.bloompipeline[up]);
        SDL_BindGPUFragmentSamplers(pass, 0, &sampler, 1);
        SDL_PushGPUFragmentUniformData(gpu.command, 0, control, sizeof(control));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
        source = gpu.bloom[level];
        width = (gpu.width+(1u << (level+1))-1) >> (level+1);
        height = (gpu.height+(1u << (level+1))-1) >> (level+1);
    }
    return gpu.bloom[0];
}

void GPU_EndFrame(void)
{
    SDL_GPUColorTargetInfo color = {0};
    SDL_GPUDepthStencilTargetInfo depth = {0};
    SDL_GPURenderPass *pass;
    qboolean sceneinitialized = false;
    gpu_overlay_snapshot_t *snapshots = NULL;
    size_t i;
    float post[12] = {0};
    SDL_GPUTextureSamplerBinding samplers[2];
    double encodebegin = gpu.benchmarking ? Sys_DoubleTime() : 0;
    if (!gpu.command || !gpu.swapchain) return;
    GPU_UploadGeometry();
    GPU_LightingUpload();
    GPU_DrawShadows();
    for (gpu_panini_view_t *view = paniniviews; view; view = view->next) if (view->used && !view->populated) {
        color.texture = view->color; color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE; color.clear_color.a = 1;
        pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, NULL);
        if (!pass) GPU_Error("clear Panini source");
        SDL_EndGPURenderPass(pass);
    }
    /* Consecutive draws retain their destination. A target may be sampled after
       its pass ends, and resumed later without discarding earlier contents. */
    for (i = 0; i < gpu.numdraws;) {
        size_t first = i, j, end;
        gpu_draw_t *initial = &gpu.draws[i];
        qboolean seen = false;
        if (initial->shadowonly) { ++i; continue; }
        if (gpu.sceneonlygamma && initial->screenoverlay) {
            GPU_SnapshotOverlay(i, &snapshots);
            ++i; continue;
        }
        for (j = 0; j < first; ++j)
            if (!gpu.draws[j].shadowonly && !(gpu.sceneonlygamma && gpu.draws[j].screenoverlay) &&
                gpu.draws[j].target == initial->target) { seen = true; break; }
        for (end = i+1; end < gpu.numdraws; ++end)
            if (!gpu.draws[end].shadowonly && ((gpu.sceneonlygamma && gpu.draws[end].screenoverlay) ||
                gpu.draws[end].target != initial->target)) break;
        /* All source passes in this group have ended. Generate the filtered
         * levels before opening the destination pass, including empty views.
         * Bilinear compatibility mode samples level zero and skips this work. */
        for (j = first; j < end; ++j) {
            gpu_draw_t *d = &gpu.draws[j];
            if (d->panini && d->projection[4] && d->projection[5])
                SDL_GenerateMipmapsForGPUTexture(gpu.command, d->samplers[0].texture);
        }
        color.texture = initial->target; color.load_op = seen ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE; color.clear_color.a = 1;
        depth.texture = initial->depth; depth.clear_depth = 1;
        depth.load_op = seen ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE; depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, &depth);
        if (!pass) GPU_Error("begin material render pass");
        GPU_DrawCommands(pass, first, end, false);
        i = end;
        if (initial->target == gpu.scene) sceneinitialized = true;
        SDL_EndGPURenderPass(pass);
    }
    if (!sceneinitialized) {
        color.texture = gpu.scene; color.load_op = SDL_GPU_LOADOP_CLEAR; color.store_op = SDL_GPU_STOREOP_STORE;
        pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, NULL); SDL_EndGPURenderPass(pass);
    }
    samplers[0].texture = gpu.scene; samplers[0].sampler = gpu.postsampler;
    samplers[1] = samplers[0];
    if (r_bloom.value > 0 && gpu_bloom_pyramid.ival && (!gpu.sceneonlygamma || sceneinitialized)) {
        samplers[1].texture = GPU_Bloom(); post[9] = 1;
    }
    color.texture = gpu.present; color.load_op = SDL_GPU_LOADOP_DONT_CARE;
    pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, gpu.postpipeline);
    SDL_BindGPUFragmentSamplers(pass, 0, samplers, 2);
    post[0] = 1.0f/gpu.width; post[1] = 1.0f/gpu.height; post[2] = r_bloom.value;
    post[3] = v_gammainverted.ival ? v_gamma.value : 1.0f/max(0.1f, v_gamma.value);
    post[4] = v_contrast.value; post[5] = v_brightness.value; post[6] = v_contrastboost.value; post[8] = r_fxaa.value;
    if (gpu.sceneonlygamma && !sceneinitialized) {
        post[2] = post[5] = post[8] = 0;
        post[3] = post[4] = post[6] = 1;
    }
    SDL_PushGPUFragmentUniformData(gpu.command, 0, post, sizeof(post));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    if (gpu.sceneonlygamma && gpu.numdraws) {
        color.texture = gpu.present; color.load_op = SDL_GPU_LOADOP_LOAD;
        depth.texture = gpu.depth; depth.clear_depth = 1;
        depth.load_op = sceneinitialized ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
        pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, &depth);
        if (!pass) GPU_Error("begin screen overlay pass");
        GPU_DrawCommands(pass, 0, gpu.numdraws, true);
        SDL_EndGPURenderPass(pass);
    }
    {
        SDL_GPUBlitInfo blit = {0};
        blit.source.texture = gpu.present; blit.source.w = gpu.width; blit.source.h = gpu.height;
        blit.destination.texture = gpu.swapchain; blit.destination.w = gpu.width; blit.destination.h = gpu.height;
        blit.load_op = SDL_GPU_LOADOP_DONT_CARE; blit.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(gpu.command, &blit);
    }
    if (!SDL_SubmitGPUCommandBuffer(gpu.command)) GPU_Error("submit frame");
    while (snapshots) {
        gpu_overlay_snapshot_t *snapshot = snapshots;
        snapshots = snapshot->next;
        SDL_ReleaseGPUTexture(gpu.device, snapshot->texture); Z_Free(snapshot);
    }
    if (gpu.benchmarking) GPU_BenchmarkFrame(encodebegin, Sys_DoubleTime());
    gpu.command = NULL; gpu.swapchain = NULL;
    extern cvar_t in_windowed_mouse;
    if (vid.activeapp && !Key_MouseShouldBeFree() && (vid_isfullscreen || in_windowed_mouse.ival)) IN_ActivateMouse();
    else IN_DeactivateMouse();
}

void GPU_SelectMode(backendmode_t newmode) { mode = newmode; }
void GPU_SelectEntity(entity_t *e) { entity = e ? e : &r_worldentity; }
void GPU_Scissor(srect_t *rect) { hasscissor = rect != NULL; if (rect) scissor = *rect; }

static void GPU_EntityMatrix(float *m)
{
    unsigned i, j;
    Matrix4x4_Identity(m);
    if (gpu.drawing2d || entity == &r_worldentity) return;
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) m[i*4+j] = entity->axis[i][j];
        m[12+i] = entity->origin[i];
    }
#ifdef PEXT_SCALE
    if (entity->scale != 0 && entity->scale != 1)
        for (i = 0; i < 3; ++i) for (j = 0; j < 3; ++j) m[i*4+j] *= entity->scale;
#endif
    if ((entity->flags & RF_WEAPONMODEL) && r_refdef.playerview && r_refdef.playerview->viewentity > 0) {
        float weapon[16], result[16];
        const vec3_t *matrix = (entity->flags & RF_WEAPONMODELNOBOB) ? r_refdef.weaponmatrix : r_refdef.weaponmatrix_bob;
        Matrix4x4_Identity(weapon);
        for (i = 0; i < 4; ++i) for (j = 0; j < 3; ++j) weapon[i*4+j] = matrix[i][j];
        Matrix4_Multiply(weapon, m, result); memcpy(m, result, sizeof(result));
    }
}

static float GPU_Wave(const shaderfunc_t *func, float time)
{
    float x = func->args[2] + time*func->args[3], value;
    x -= floor(x);
    switch (func->type) {
    case SHADER_FUNC_SQUARE: value = x < 0.5f ? 1 : -1; break;
    case SHADER_FUNC_TRIANGLE: value = 1 - fabs(x*4-2); break;
    case SHADER_FUNC_SAWTOOTH: value = x; break;
    case SHADER_FUNC_INVERSESAWTOOTH: value = 1-x; break;
    case SHADER_FUNC_NOISE: value = (rand()/(float)RAND_MAX)*2-1; break;
    case SHADER_FUNC_CONSTANT: return func->args[0];
    default: value = sin(x*2*M_PI);
    }
    return func->args[0] + value*func->args[1];
}

static void GPU_TexCoord(float uv[2], shaderpass_t *p, float time, const float pos[3])
{
    int i;
    for (i = 0; i < p->numtcmods; ++i) {
        tcmod_t *m = &p->tcmods[i];
        float u = uv[0], v = uv[1], a, b;
        switch (m->type) {
        case SHADER_TCMOD_SCALE: uv[0] *= m->args[0]; uv[1] *= m->args[1]; break;
        case SHADER_TCMOD_SCROLL: uv[0] += time*m->args[0]; uv[1] += time*m->args[1]; break;
        case SHADER_TCMOD_ROTATE:
            /* The parser stores signed turns per second, not degrees. */
            a = time*m->args[0]*2*M_PI; b = sin(a); a = cos(a);
            uv[0] = (u-0.5f)*a - (v-0.5f)*b + 0.5f; uv[1] = (u-0.5f)*b + (v-0.5f)*a + 0.5f; break;
        case SHADER_TCMOD_TRANSFORM:
            uv[0] = u*m->args[0]+v*m->args[2]+m->args[4]; uv[1] = u*m->args[1]+v*m->args[3]+m->args[5]; break;
        case SHADER_TCMOD_TURB:
            uv[0] += sin((pos[0]+pos[2])/1024 + (m->args[2]+time*m->args[3])*2*M_PI)*m->args[1];
            uv[1] += sin(pos[1]/1024 + (m->args[2]+time*m->args[3])*2*M_PI)*m->args[1]; break;
        case SHADER_TCMOD_STRETCH: {
            shaderfunc_t f = {0};
            f.type = m->args[0]; memcpy(f.args, m->args+1, 4*sizeof(float));
            a = GPU_Wave(&f, time); a = a ? 1/a : 1;
            uv[0] = (u-0.5f)*a+0.5f; uv[1] = (v-0.5f)*a+0.5f;
            break;
        }
        case SHADER_TCMOD_PAGE: {
            unsigned columns = max(1, m->args[0]), rows = max(1, m->args[1]);
            unsigned frame = (unsigned)max(0, time/max(0.0001f,m->args[2])) % (columns*rows);
            uv[0] += (frame%columns)/(float)columns; uv[1] += (frame/columns)/(float)rows;
            break;
        }
        default: break;
        }
    }
}

static texid_t GPU_PassTexture(shaderpass_t *p, texnums_t *t, float time)
{
    switch (p->texgen) {
    case T_GEN_SINGLEMAP: return p->anim_frames[0];
    case T_GEN_DIFFUSE: return t->base;
    case T_GEN_NORMALMAP: return t->bump;
    case T_GEN_SPECULAR: return t->specular;
    case T_GEN_FULLBRIGHT: return t->fullbright;
    case T_GEN_UPPEROVERLAY: return t->upperoverlay;
    case T_GEN_LOWEROVERLAY: return t->loweroverlay;
    case T_GEN_SOURCECOLOUR: {
        unsigned w, h; return R2D_RT_GetTexture(r_refdef.rt_sourcecolour.texname, &w, &h);
    }
    case T_GEN_SOURCEDEPTH: {
        unsigned w, h; return R2D_RT_GetTexture(r_refdef.rt_depth.texname, &w, &h);
    }
    case T_GEN_LIGHTMAP: return batchlightmap >= 0 && batchlightmap < numlightmaps ? lightmap[batchlightmap]->lightmap_texture : r_whiteimage;
    case T_GEN_ANIMMAP: return p->anim_numframes ? p->anim_frames[(unsigned)max(0, time*p->anim_fps)%p->anim_numframes] : t->base;
#ifdef HAVE_MEDIA_DECODER
    case T_GEN_VIDEOMAP: return Media_UpdateForShader(p->cin);
#endif
    default: return p->anim_numframes ? p->anim_frames[0] : t->base;
    }
}

void R_FetchPlayerColour(unsigned int color, vec3_t rgb)
{
    unsigned i;
    if (color >= 16) {
        rgb[0] = ((color>>16)&255)/255.0f;
        rgb[1] = ((color>>8)&255)/255.0f;
        rgb[2] = (color&255)/255.0f;
    } else {
        i = (color<<4) + (color < 8 ? 15 : 0);
        rgb[0] = host_basepal[i*3]/255.0f;
        rgb[1] = host_basepal[i*3+1]/255.0f;
        rgb[2] = host_basepal[i*3+2]/255.0f;
    }
}

unsigned GPU_ShadowSlot(dlight_t *l, float params[4])
{
    qboolean world = l >= cl_dlights+RTL_FIRST;
    unsigned slot;
    if (!r_shadow_shadowmapping.ival || (l->flags & LFLAG_NOSHADOWS) || l->radius <= 2 ||
        (world ? !r_shadow_realtime_world_shadows.ival : !r_shadow_realtime_dlight_shadows.ival)) return 0;
    float nearclip = bound(0.1f,l->nearclip ? l->nearclip : r_shadow_shadowmapping_nearclip.value,l->radius*0.5f);
    for (slot = 0; slot < numshadowlights; ++slot)
        if (shadowlights[slot].light == l && shadowlights[slot].radius == l->radius &&
            shadowlights[slot].nearclip == nearclip && !memcmp(shadowlights[slot].origin,l->origin,sizeof(vec3_t))) break;
    if (slot == GPU_MAX_SHADOWS) return 0;
    if (slot == numshadowlights) {
        shadowlights[slot].light = l;
        VectorCopy(l->origin,shadowlights[slot].origin); shadowlights[slot].radius = l->radius;
        shadowlights[slot].nearclip = nearclip;
        ++numshadowlights;
    }
    params[0] = shadowlights[slot].nearclip; params[1] = l->radius;
    params[2] = max(0.001f,r_shadow_shadowmapping_bias.value); params[3] = 0;
    return slot+1;
}

static void GPU_Lights(gpu_draw_t *d, unsigned flags, const mesh_t *mesh, const vecV_t *coords)
{
    unsigned i, count = 0, selected[GPU_MAX_LIGHTS], numselected = 0;
    float scores[GPU_MAX_LIGHTS];
    vec3_t mins = {FLT_MAX,FLT_MAX,FLT_MAX}, maxs = {-FLT_MAX,-FLT_MAX,-FLT_MAX};
    if (gpu.drawing2d || flags & BEF_NODLIGHT ||
        (d->frag.material[0] != 1 && d->frag.material[0] != 2) ||
        (!r_shadow_realtime_world.ival && !r_shadow_realtime_dlight.ival)) return;
    if (GPU_ClusterLights(d)) return;
    for (i = 0; i < mesh->numvertexes; ++i) {
        vec3_t point;
        unsigned j;
        for (j = 0; j < 3; ++j) {
            point[j] = d->vert.model[j]*coords[i][0]+d->vert.model[4+j]*coords[i][1]+
                d->vert.model[8+j]*coords[i][2]+d->vert.model[12+j];
            mins[j] = min(mins[j],point[j]); maxs[j] = max(maxs[j],point[j]);
        }
    }
    /* Rank intersecting lights per draw instead of taking the first 32 lights
       in a map. Large rtlights files must not hide nearby lights arbitrarily. */
    for (i = 0; i < rtlights_max; ++i) {
        dlight_t *l = &cl_dlights[i];
        float distance = 0, score, style = 1;
        unsigned j, slot;
        qboolean isworld = i >= RTL_FIRST;
        if (!l->radius) continue; /* CL_DecayLights owns expiry; CSQC lights can expire next frame. */
        if (isworld ? !r_shadow_realtime_world.ival : !r_shadow_realtime_dlight.ival) continue;
        if (!(l->flags & (isworld ? LFLAG_REALTIMEMODE : LFLAG_NORMALMODE))) continue;
        if (R_CullSphere(l->origin,l->radius)) continue;
        for (j = 0; j < 3; ++j) {
            float delta = l->origin[j]-bound(mins[j],l->origin[j],maxs[j]);
            distance += delta*delta;
        }
        if (distance >= l->radius*l->radius) continue;
        if (l->style >= 0 && l->style < MAX_NET_LIGHTSTYLES) style = d_lightstylevalue[l->style]/256.0f;
        score = style*(fabs(l->color[0])+fabs(l->color[1])+fabs(l->color[2]))*
            (1-distance/(l->radius*l->radius));
        if (score <= 0) continue;
        for (slot = 0; slot < numselected && scores[slot] >= score; ++slot) {}
        if (slot == GPU_MAX_LIGHTS) continue;
        if (numselected < GPU_MAX_LIGHTS) ++numselected;
        for (j = numselected-1; j > slot; --j) { scores[j] = scores[j-1]; selected[j] = selected[j-1]; }
        scores[slot] = score; selected[slot] = i;
    }
    for (count = 0; count < numselected; ++count) {
        dlight_t *l = &cl_dlights[selected[count]];
        float style = 1;
        if (l->style >= 0 && l->style < MAX_NET_LIGHTSTYLES) style = d_lightstylevalue[l->style]/256.0f;
        VectorCopy(l->origin, d->frag.lightpos[count]); d->frag.lightpos[count][3] = l->radius;
        VectorScale(l->color, style, d->frag.lightcolor[count]);
        VectorCopy(l->lightcolourscales, d->frag.lightparams[count]);
        d->frag.lightcolor[count][3] = GPU_ShadowSlot(l,d->frag.lightshadow[count]);
    }
    d->frag.material[3] = count;
}

static void GPU_DrawMesh(shader_t *shader, mesh_t *mesh, vbo_t *vbo, texnums_t *textures, unsigned flags, shaderpass_t *p, const gpu_draw_t *prepared)
{
    size_t i;
    unsigned base = 0;
    qboolean appended = false;
    gpu_draw_t *d;
    program_t *program = p->prog ? p->prog : shader->prog;
    /* BSP vertex-lit variants use baked mesh colours, including brush models. */
    qboolean vertexlit = program && !strncmp(program->name, "defaultwall", 11) &&
        strstr(program->name, "#VERTEXLIT");
    float mv[16];
    float time = r_refdef.time - entity->shaderTime;
    vecV_t *coords = mesh->xyz_array ? mesh->xyz_array : vbo ? (vecV_t *)vbo->coord.sysptr + mesh->vbofirstvert : NULL;
    vec2_t *uvs = mesh->st_array ? mesh->st_array : vbo ? (vec2_t *)vbo->texcoord.sysptr + mesh->vbofirstvert : NULL;
    vec3_t *normals = mesh->normals_array ? mesh->normals_array : vbo && vbo->normals.sysptr ? (vec3_t *)vbo->normals.sysptr+mesh->vbofirstvert : NULL;
    vec3_t *tangents = mesh->snormals_array ? mesh->snormals_array : vbo && vbo->svector.sysptr ? (vec3_t *)vbo->svector.sysptr+mesh->vbofirstvert : NULL;
    vec3_t *bitangents = mesh->tnormals_array ? mesh->tnormals_array : vbo && vbo->tvector.sysptr ? (vec3_t *)vbo->tvector.sysptr+mesh->vbofirstvert : NULL;
    vec2_t *lmuvs = mesh->lmst_array[0] ? mesh->lmst_array[0] : vbo && vbo->lmcoord[0].sysptr ? (vec2_t *)vbo->lmcoord[0].sysptr+mesh->vbofirstvert : NULL;
    avec4_t *colors = mesh->colors4f_array[0] ? mesh->colors4f_array[0] :
        vbo && !vbo->colours_bytes && vbo->colours[0].sysptr ? (avec4_t *)vbo->colours[0].sysptr+mesh->vbofirstvert : NULL;
    byte_vec4_t *bytecolors = mesh->colors4b_array ? mesh->colors4b_array :
        vbo && vbo->colours_bytes && vbo->colours[0].sysptr ? (byte_vec4_t *)vbo->colours[0].sysptr+mesh->vbofirstvert : NULL;
    index_t *indices = mesh->indexes ? mesh->indexes : vbo ? (index_t *)vbo->indicies.sysptr + mesh->vbofirstelement : NULL;
    if (!coords || !indices || mesh->numvertexes <= 0 || mesh->numindexes <= 0 || !gpu.command) return;
    if (mode != BEM_STANDARD && mode != BEM_DEPTHONLY && mode != BEM_DEPTHDARK) return;
    /* These generators leave the packed world attributes invariant. Anything
     * animated, entity-coloured, vertex-lit or using generated UVs streams. */
    qboolean immutable = gpu_staticgeometry.ival && !staticinvalid && entity == &r_worldentity &&
        entity->model == staticworld && !gpu.drawing2d && !vertexlit && program &&
        !shader->numdeforms && !mesh->xyz2_array && !mesh->bones &&
        mesh->numvertexes <= GPU_STATIC_VERTICES && !p->numtcmods && p->tcgen == TC_GEN_BASE &&
        p->texgen != T_GEN_LIGHTMAP &&
        (p->rgbgen == RGB_GEN_IDENTITY || p->rgbgen == RGB_GEN_IDENTITY_LIGHTING ||
         p->rgbgen == RGB_GEN_IDENTITY_OVERBRIGHT || p->rgbgen == RGB_GEN_LIGHTING_DIFFUSE) &&
        (p->alphagen == ALPHA_GEN_IDENTITY || p->alphagen == ALPHA_GEN_UNDEFINED) &&
        !(flags & (BEF_FORCECOLOURMOD|BEF_FORCETRANSPARENT|BEF_ENTITYCOLOURS));
    gpu_static_mesh_t *cached = immutable ? GPU_StaticMesh(mesh) : NULL;
    SDL_GPUBuffer *vertexbuffer = cached ? cached->page->buffer : NULL;
    if (!cached && gpu.numstreamvertices + mesh->numvertexes > gpu.maxvertices) {
        gpu.maxvertices = max(gpu.numstreamvertices+mesh->numvertexes, max(4096, gpu.maxvertices*2));
        gpu.cpuvertices = BZ_Realloc(gpu.cpuvertices, gpu.maxvertices*sizeof(*gpu.cpuvertices));
    }
    if (gpu.numindices + mesh->numindexes > gpu.maxindices) {
        gpu.maxindices = max(gpu.numindices+mesh->numindexes, max(8192, gpu.maxindices*2));
        gpu.cpuindices = BZ_Realloc(gpu.cpuindices, gpu.maxindices*sizeof(*gpu.cpuindices));
    }
    /* Within a material pass, non-RT uniforms don't depend on the mesh.
     * Append geometry directly when the prepared state and index range match. */
    d = gpu.numdraws ? &gpu.draws[gpu.numdraws-1] : NULL;
    if (prepared && d && (!immutable || cached) && d->vertexbuffer == vertexbuffer &&
        d->firstindex+d->numindices == gpu.numindices &&
        (cached || sizeof(index_t)>2 || gpu.numstreamvertices-d->vertexoffset+mesh->numvertexes <= 65536) &&
        !memcmp(&d->bits,&prepared->bits,sizeof(*d)-offsetof(gpu_draw_t,bits))) {
        base = cached ? cached->first : gpu.numstreamvertices - d->vertexoffset;
        d->numindices += mesh->numindexes;
        appended = true;
    } else {
        if (gpu.numdraws == gpu.maxdraws) {
            gpu.maxdraws = max(256, gpu.maxdraws*2);
            gpu.draws = BZ_Realloc(gpu.draws, gpu.maxdraws*sizeof(*gpu.draws));
        }
        d = &gpu.draws[gpu.numdraws++];
        if (prepared) memcpy(d, prepared, sizeof(*d));
        else memset(d, 0, sizeof(*d));
        d->firstindex = gpu.numindices; d->numindices = mesh->numindexes;
        d->vertexoffset = cached ? 0 : gpu.numstreamvertices;
        d->vertexbuffer = vertexbuffer; d->boundsvalid = false;
        if (cached) base = cached->first;
    }
    if (!prepared) {
        d->shadowonly = collectshadowonly;
        d->vertexlit = vertexlit;
        /* Offscreen 2D images must remain in submission order: later world
         * and UI draws may sample them. Only main-screen overlays bypass gamma. */
        d->screenoverlay = gpu.drawing2d && gpu.target == gpu.scene;
        d->target = gpu.target; d->depth = gpu.targetdepth; d->format = gpu.targetformat;
        d->castshadow = !gpu.drawing2d && !(flags & BEF_NOSHADOWS) &&
            !(entity->flags & (RF_NOSHADOW|RF_WEAPONMODEL)) &&
            !(shader->flags & (SHADER_SKY|SHADER_NOSHADOWS)) &&
            (!(p->shaderbits & SBITS_BLEND_BITS) ||
             (p->shaderbits & SBITS_BLEND_BITS) == (SBITS_SRCBLEND_ONE|SBITS_DSTBLEND_ZERO) ||
             (p->shaderbits & SBITS_ATEST_BITS)) &&
            (entity != &r_worldentity || collectshadowonly);
        d->bits = p->shaderbits;
        if (gpu.drawing2d || flags & BEF_FORCENODEPTH) d->bits |= SBITS_MISC_NODEPTHTEST;
        if (flags & BEF_FORCEDEPTHWRITE) d->bits |= SBITS_MISC_DEPTHWRITE;
        if (flags & BEF_FORCETRANSPARENT) d->bits = (d->bits & ~(SBITS_BLEND_BITS|SBITS_MISC_DEPTHWRITE)) | SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
        if (flags & BEF_FORCEADDITIVE) d->bits = (d->bits & ~(SBITS_BLEND_BITS|SBITS_MISC_DEPTHWRITE)) | SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE;
        if (mode == BEM_DEPTHONLY) {
            /* Sky visibility masks must write only their actual BSP depth.
             * Drawing their color here overwrites the projected clouds with
             * tiled brush UVs after R_DrawSkyChain has drawn the sky. */
            d->bits &= ~(SBITS_BLEND_BITS|SBITS_DEPTHFUNC_BITS|SBITS_MISC_NODEPTHTEST);
            d->bits |= SBITS_MISC_DEPTHWRITE|SBITS_MASK_BITS;
        }
        d->cull = gpu.drawing2d || flags & BEF_FORCETWOSIDED ? 0 : shader->flags & SHADER_CULL_FLIP;
        if (r_refdef.flipcull && d->cull) d->cull ^= SHADER_CULL_FLIP;
        d->lines = (flags & BEF_LINES) || (p->shaderbits & SBITS_LINES);
        d->viewport.x = r_refdef.pxrect.x; d->viewport.y = r_refdef.pxrect.y;
        d->viewport.w = r_refdef.pxrect.width; d->viewport.h = r_refdef.pxrect.height;
        d->viewport.min_depth = 0; d->viewport.max_depth = entity->flags & RF_DEPTHHACK ? 0.3f : 1;
        d->scissor.x = 0; d->scissor.y = 0; d->scissor.w = gpu.targetwidth; d->scissor.h = gpu.targetheight;
        if (hasscissor) {
            d->scissor.x = max(0, scissor.x*gpu.targetwidth); d->scissor.y = max(0, (1-scissor.y-scissor.height)*gpu.targetheight);
            d->scissor.w = max(1, scissor.width*gpu.targetwidth); d->scissor.h = max(1, scissor.height*gpu.targetheight);
        }
        GPU_EntityMatrix(d->vert.model);
        Matrix4_Multiply(r_refdef.m_view, d->vert.model, mv);
        Matrix4_Multiply(entity->flags & RF_DEPTHHACK ? r_refdef.m_projection_view : r_refdef.m_projection_std, mv, d->vert.mvp);
        if (gpu.drawing2d) {
            /* Orthographic helper emits [-1,1] depth; SDL GPU uses [0,1]. */
            for (i = 0; i < 4; ++i) d->vert.mvp[i*4+2] = (d->vert.mvp[i*4+2]+d->vert.mvp[i*4+3])*0.5f;
        }
        VectorCopy(r_refdef.vieworg, d->frag.eye); d->frag.eye[3] = time;
        d->frag.material[0] = 0;
        if (!gpu.drawing2d && (p->rgbgen == RGB_GEN_LIGHTING_DIFFUSE || p->rgbgen == RGB_GEN_ENTITY_LIGHTING_DIFFUSE))
            d->frag.material[0] = 2;
        if (!gpu.drawing2d && program &&
            (!strncmp(program->name, "defaultwall", 11) || !strncmp(program->name, "defaultskin", 11)))
            d->frag.material[0] = vertexlit || (shader->usageflags & SUF_LIGHTMAP) ? 1 : 2;
        if (!gpu.drawing2d && program) {
            if (!strncmp(program->name, "defaultwarp", 11)) d->frag.material[0] = 3;
            if (!strncmp(program->name, "defaultskybox", 13)) d->frag.material[0] = 6;
            else if (!strncmp(program->name, "defaultsky", 10)) d->frag.material[0] = 4;
        }
        d->frag.material[1] = r_lightmap_scale.value * r_refdef.hdr_value;
        /* CPU lightmap packing divides out overbright bits; restore them when sampling. */
        if (!vertexlit && entity->model && (gl_overbright_all.ival || (entity->model->engineflags & MDLF_NEEDOVERBRIGHT)))
            d->frag.material[1] *= 1 << bound(0, gl_overbright.ival, 2);
        if (r_shadow_realtime_world.ival && d->frag.material[0] == 1) d->frag.material[1] *= r_shadow_realtime_world_lightmaps.value;
        d->frag.material[2] = (d->bits & SBITS_ATEST_BITS) >> SBITS_ATEST_SHIFT;
        d->frag.effects[0] = r_normalmapping.ival;
        d->frag.effects[1] = gl_specular.value; d->frag.effects[2] = r_glsl_offsetmapping.ival ? r_glsl_offsetmapping_scale.value : 0;
        d->frag.effects[3] = max(1, gl_specular_power.value);
        VectorCopy(entity->light_avg, d->frag.ambient); VectorCopy(entity->light_range, d->frag.diffuse);
        for (i = 0; i < 3; ++i)
            d->frag.lightdir[i] = entity->light_dir[0]*entity->axis[0][i] +
                entity->light_dir[1]*entity->axis[1][i] + entity->light_dir[2]*entity->axis[2][i];
        R_FetchPlayerColour(entity->topcolour, d->frag.upper); R_FetchPlayerColour(entity->bottomcolour, d->frag.lower);
        if (program) {
            const char *mask = strstr(program->name, "#MASK=");
            if (mask) {
                /* Builtin cutout permutations perform their alpha test in the
                 * program rather than setting a fixed-function alphafunc bit. */
                /* Sprite MASK rejects transparent texels before vertex fading.
                 * Wall/skin MASK permutations also support the inverse pass. */
                d->frag.material[2] = !strncmp(program->name, "defaultsprite", 13) ? 6 :
                    strstr(program->name, "#MASKLT") ? 4 : 5;
                d->frag.upper[3] = bound(0, atof(mask+6), 1);
            }
        }
        if (!gpu.drawing2d) {
            VectorCopy(r_refdef.globalfog.colour, d->frag.fog); d->frag.fog[3] = r_refdef.globalfog.density;
        }
        d->samplers[0] = GPU_TextureBinding(p->texgen == T_GEN_LIGHTMAP && r_fullbright.value > 0 ?
            r_whiteimage : GPU_PassTexture(p, textures, time), missing_texture);
        d->samplers[1] = GPU_TextureBinding(textures->bump, missing_texture_normal);
        d->samplers[2] = GPU_TextureBinding(textures->specular, missing_texture_gloss);
        d->samplers[3] = GPU_TextureBinding(textures->fullbright, r_blackimage);
        d->samplers[4] = GPU_TextureBinding(!vertexlit && batchlightmap >= 0 && batchlightmap < numlightmaps ? lightmap[batchlightmap]->lightmap_texture : NULL, r_whiteimage);
        if (d->frag.material[0] == 1 && r_fullbright.value > 0) {
            /* Prebaked Q3 lightmaps aren't rebuilt when fullbright changes. */
            d->samplers[4] = GPU_TextureBinding(NULL, r_whiteimage);
            d->frag.material[1] = r_fullbright.value * r_lightmap_scale.value * r_refdef.hdr_value;
        }
        d->samplers[5] = GPU_TextureBinding(textures->upperoverlay, r_blackimage);
        d->samplers[6] = GPU_TextureBinding(textures->loweroverlay, r_blackimage);
        d->samplers[8] = GPU_TextureBinding(NULL, missing_texture_normal);
        if (!vertexlit && batchlightmap >= 0 && batchlightmap+1 < numlightmaps && lightmap[batchlightmap]->hasdeluxe && r_deluxemapping_cvar.ival) {
            d->samplers[8] = GPU_TextureBinding(lightmap[batchlightmap+1]->lightmap_texture, missing_texture_normal);
            /* Quake .lux data is in the surface tangent basis. Q3 may instead
             * declare model-space directions; do not dot tangent data with world normals. */
            d->frag.settings[0] = entity->model && entity->model->lightmaps.deluxemapping_modelspace ? 2 : 1;
        }
        d->samplers[7].texture = gpu.shadowmap; d->samplers[7].sampler = gpu.shadowsampler;
        d->samplers[9] = d->samplers[10] = GPU_TextureBinding(NULL, r_blackimage);
        d->samplers[11].texture = gpu.fallbackcube; d->samplers[11].sampler = gpu.postsampler;
        if (d->frag.material[0] == 6) {
            SDL_GPUTextureSamplerBinding cube = GPU_TextureBinding(textures->reflectcube, NULL);
            if (cube.texture && textures->reflectcube->gpuimage->type == SDL_GPU_TEXTURETYPE_CUBE)
                d->samplers[11] = cube;
            /* A cube pass must never be bound to the scene shader's 2D base slot. */
            d->samplers[0] = GPU_TextureBinding(NULL, r_blackimage);
        }
        if (clipenabled) {
            d->frag.settings[1] = 1;
            memcpy(d->frag.clipplane, clipplane, sizeof(clipplane));
        }
        if (currentwater) {
            d->frag.material[0] = 5;
            /* The material generator resolves map/entity overrides and fluid cvars.
             * Carry that opacity for every liquid, including custom mod textures. */
            d->frag.effects[0] = 0;
            if (program) {
                const char *alpha = strstr(program->name, "#ALPHA=");
                if (alpha) d->frag.effects[0] = bound(0, atof(alpha+7), 1);
            }
            d->frag.settings[2] = !!(currentwater->flags & SHADER_HASREFLECT);
            d->frag.settings[3] = !!(currentwater->flags & SHADER_HASREFRACT);
            memcpy(d->frag.screenmatrix, currentwater->screenmatrix, sizeof(currentwater->screenmatrix));
            if (currentwater->flags & SHADER_HASREFLECT) {
                d->samplers[9].texture = currentwater->color[0]; d->samplers[9].sampler = gpu.postsampler;
            }
            if (currentwater->flags & SHADER_HASREFRACT) {
                d->samplers[10].texture = currentwater->color[1]; d->samplers[10].sampler = gpu.postsampler;
            }
            /* Water programs' first pass names a generated view, not the albedo. */
            d->samplers[0] = GPU_TextureBinding(textures->base, missing_texture);
        }
        if (!collectshadowonly) GPU_Lights(d, flags | (shader->flags & SHADER_NODLIGHT ? BEF_NODLIGHT : 0), mesh, coords);
    }
    memcpy(gpu.cpuindices + gpu.numindices, indices, mesh->numindexes*sizeof(index_t));
    if (base || (vbo && !mesh->indexes)) {
        unsigned adjust = vbo && !mesh->indexes ? mesh->vbofirstvert : 0;
        for (i = 0; i < mesh->numindexes; ++i) gpu.cpuindices[gpu.numindices+i] += base-adjust;
    }
    for (i = 0; !cached && i < mesh->numvertexes; ++i) {
        gpu_vertex_t *v = &gpu.cpuvertices[gpu.numstreamvertices+i];
        int j;
        memset(v, 0, sizeof(*v));
        VectorCopy(coords[i], v->position);
        if (mesh->xyz2_array) for (j = 0; j < 3; ++j)
            v->position[j] = coords[i][j]*mesh->xyz_blendw[0] + mesh->xyz2_array[i][j]*mesh->xyz_blendw[1];
        if (normals) VectorCopy(normals[i], v->normal); else v->normal[2] = 1;
        if (tangents) VectorCopy(tangents[i], v->tangent); else v->tangent[0] = 1;
        if (bitangents) VectorCopy(bitangents[i], v->bitangent); else v->bitangent[1] = 1;
        if (mesh->bones && mesh->bonenums && mesh->boneweights) {
            vec3_t position = {0,0,0}, normal = {0,0,0}, tangent = {0,0,0}, bitangent = {0,0,0};
            for (j = 0; j < 4; ++j) {
                const float *b = mesh->bones + 12*mesh->bonenums[i][j];
                float w = mesh->boneweights[i][j];
                position[0] += w*(DotProduct(b, v->position)+b[3]);
                position[1] += w*(DotProduct(b+4, v->position)+b[7]);
                position[2] += w*(DotProduct(b+8, v->position)+b[11]);
                int k;
                for (k = 0; k < 3; ++k) {
                    normal[k] += w*DotProduct(b+4*k,v->normal);
                    tangent[k] += w*DotProduct(b+4*k,v->tangent);
                    bitangent[k] += w*DotProduct(b+4*k,v->bitangent);
                }
            }
            VectorCopy(position, v->position);
            VectorCopy(normal, v->normal); VectorCopy(tangent, v->tangent); VectorCopy(bitangent, v->bitangent);
        }
        for (j = 0; j < shader->numdeforms; ++j) {
            deformv_t *deform = &shader->deforms[j];
            float amount;
            switch (deform->type) {
            case DEFORMV_WAVE: {
                shaderfunc_t f = deform->func;
                f.args[2] += deform->args[0]*(v->position[0]+v->position[1]+v->position[2]);
                amount = GPU_Wave(&f, time);
                VectorMA(v->position,amount,v->normal,v->position); break;
            }
            case DEFORMV_MOVE:
                amount = GPU_Wave(&deform->func,time);
                VectorMA(v->position,amount,deform->args,v->position); break;
            case DEFORMV_BULGE:
                amount = sin((uvs ? uvs[i][0] : 0)*deform->args[0]+time*deform->args[2])*deform->args[1];
                VectorMA(v->position,amount,v->normal,v->position); break;
            default: break;
            }
        }
        if (uvs) { v->uv[0] = uvs[i][0]; v->uv[1] = uvs[i][1]; }
        if (lmuvs) { v->lmuv[0] = lmuvs[i][0]; v->lmuv[1] = lmuvs[i][1]; }
        if (p->tcgen == TC_GEN_LIGHTMAP) memcpy(v->uv, v->lmuv, sizeof(v->uv));
        if (p->tcgen == TC_GEN_ENVIRONMENT) {
            vec3_t camera, direction, reflected;
            VectorSubtract(r_refdef.vieworg,entity->origin,camera);
            for (j = 0; j < 3; ++j) direction[j] = DotProduct(camera,entity->axis[j])-v->position[j];
            VectorNormalize(direction);
            float dot = DotProduct(v->normal,direction);
            for (j = 0; j < 3; ++j) reflected[j] = 2*dot*v->normal[j]-direction[j];
            v->uv[0] = 0.5f+reflected[1]*0.5f; v->uv[1] = 0.5f-reflected[2]*0.5f;
        }
        if (p->tcgen == TC_GEN_VECTOR) { v->uv[0] = DotProduct(v->position, p->tcgenvec[0]); v->uv[1] = DotProduct(v->position, p->tcgenvec[1]); }
        if (d->frag.material[0] != 3 && d->frag.material[0] != 4) GPU_TexCoord(v->uv, p, time, v->position);
        for (j = 0; j < 4; ++j) v->color[j] = 1;
        if (vertexlit || p->rgbgen == RGB_GEN_VERTEX_EXACT || p->rgbgen == RGB_GEN_VERTEX_LIGHTING || p->rgbgen == RGB_GEN_ONE_MINUS_VERTEX || p->alphagen == ALPHA_GEN_VERTEX) {
            if (colors) memcpy(v->color, colors[i], sizeof(v->color));
            else if (bytecolors) for (j = 0; j < 4; ++j) v->color[j] = bytecolors[i][j]/255.0f;
            if (p->rgbgen == RGB_GEN_ONE_MINUS_VERTEX) for (j = 0; j < 3; ++j) v->color[j] = 1-v->color[j];
        }
        for (j = 0; j < 3; ++j) {
            if (p->rgbgen == RGB_GEN_ENTITY || flags & BEF_FORCECOLOURMOD) v->color[j] *= entity->shaderRGBAf[j];
            if (p->rgbgen == RGB_GEN_ONE_MINUS_ENTITY) v->color[j] *= 1-entity->shaderRGBAf[j];
            if (p->rgbgen == RGB_GEN_CONST) v->color[j] *= p->rgbgen_func.args[j];
            if (p->rgbgen == RGB_GEN_WAVE) v->color[j] *= bound(0,GPU_Wave(&p->rgbgen_func, time),1);
            if (p->rgbgen == RGB_GEN_TOPCOLOR) v->color[j] *= d->frag.upper[j];
            if (p->rgbgen == RGB_GEN_BOTTOMCOLOR) v->color[j] *= d->frag.lower[j];
            if (p->rgbgen == RGB_GEN_ENTITY_LIGHTING_DIFFUSE) v->color[j] *= entity->shaderRGBAf[j];
        }
        if (p->alphagen == ALPHA_GEN_ENTITY || flags & BEF_FORCETRANSPARENT) {
            /* Sprite batches select the world entity; their original alpha is
             * carried by each vertex instead of that entity's opaque RGBA. */
            float alpha = entity->shaderRGBAf[3];
            if (flags & BEF_ENTITYCOLOURS) {
                if (colors) alpha = colors[i][3];
                else if (bytecolors) alpha = bytecolors[i][3]/255.0f;
                /* ALPHA_GEN_VERTEX already consumed the same baked value. */
                if (p->alphagen == ALPHA_GEN_VERTEX) alpha = 1;
            }
            v->color[3] *= alpha;
        }
        if (vertexlit && r_fullbright.value > 0)
            for (j = 0; j < 3; ++j) v->color[j] = 1;
        if (!program && p->texgen == T_GEN_LIGHTMAP) {
            /* Standalone lightmap passes need the same gain as builtin walls. */
            float gain = r_fullbright.value > 0 ? r_fullbright.value : d->frag.material[1];
            for (j = 0; j < 3; ++j) v->color[j] *= gain;
        }
        if (p->alphagen == ALPHA_GEN_CONST) v->color[3] *= p->alphagen_func.args[0];
        if (p->alphagen == ALPHA_GEN_WAVE) v->color[3] *= bound(0,GPU_Wave(&p->alphagen_func, time),1);
        if (p->alphagen == ALPHA_GEN_PORTAL) {
            vec3_t worldpos, delta;
            for (j = 0; j < 3; ++j) worldpos[j] = d->vert.model[j]*v->position[0]+
                d->vert.model[4+j]*v->position[1]+d->vert.model[8+j]*v->position[2]+d->vert.model[12+j];
            VectorSubtract(r_refdef.vieworg,worldpos,delta);
            v->color[3] *= r_refdef.recurse ? 1 : bound(0,VectorLength(delta)/max(1,shader->portaldist),1);
        }
        if (p->alphagen == ALPHA_GEN_SPECULAR) {
            vec3_t camera, delta;
            VectorSubtract(r_refdef.vieworg,entity->origin,camera);
            for (j = 0; j < 3; ++j) delta[j] = DotProduct(camera,entity->axis[j])-v->position[j];
            VectorNormalize(delta);
            float shine = max(0,DotProduct(delta,v->normal));
            v->color[3] *= shine*shine*shine*shine*shine;
        }

    }
    if (!cached && immutable) {
        cached = GPU_CacheStaticMesh(mesh, gpu.cpuvertices+gpu.numstreamvertices);
        if (cached) {
            d->vertexbuffer = cached->page->buffer; d->vertexoffset = 0;
            for (i = 0; i < mesh->numindexes; ++i) gpu.cpuindices[gpu.numindices+i] += cached->first;
        }
    }
    /* Bounds cover the final deformed/skinned positions. They are separate
     * from render state so combining draws unions bounds rather than disabling
     * batching. Immutable world vertices already use world coordinates. */
    if (cached) {
        for (i = 0; i < 3; ++i) {
            d->mins[i] = d->boundsvalid ? min(d->mins[i],cached->mins[i]) : cached->mins[i];
            d->maxs[i] = d->boundsvalid ? max(d->maxs[i],cached->maxs[i]) : cached->maxs[i];
        }
        d->boundsvalid = true; gpu.cachedvertices += mesh->numvertexes;
    } else if (d->castshadow && gpu_shadowcull.ival &&
        (r_shadow_realtime_world.ival || r_shadow_realtime_dlight.ival)) {
        for (i = 0; i < mesh->numvertexes; ++i) {
            const float *pos = gpu.cpuvertices[gpu.numstreamvertices+i].position;
            unsigned j;
            for (j = 0; j < 3; ++j) {
                float point = d->vert.model[j]*pos[0]+d->vert.model[4+j]*pos[1]+d->vert.model[8+j]*pos[2]+d->vert.model[12+j];
                d->mins[j] = d->boundsvalid ? min(d->mins[j],point) : point;
                d->maxs[j] = d->boundsvalid ? max(d->maxs[j],point) : point;
            }
            d->boundsvalid = true;
        }
    }
    /* Combine adjacent geometry without changing triangle/blend order. The
     * zero-initialized draw record after vertexoffset contains all render
     * state, texture bindings and uniforms; differing state ends a batch. */
    if (!appended && gpu.numdraws >= 2 && (gpu.batching || d->shadowonly)) {
        gpu_draw_t *prev = d-1;
        unsigned base = d->vertexoffset - prev->vertexoffset;
        qboolean same = !d->shadowonly && !prev->shadowonly &&
            !memcmp(&prev->bits, &d->bits, sizeof(*d)-offsetof(gpu_draw_t,bits));
        /* Shadow-only draws don't consume the scene lighting/texture uniforms. */
        if (d->shadowonly && prev->shadowonly && prev->castshadow == d->castshadow &&
            prev->vertexbuffer == d->vertexbuffer &&
            prev->samplers[0].texture == d->samplers[0].texture &&
            prev->samplers[0].sampler == d->samplers[0].sampler &&
            prev->frag.material[2] == d->frag.material[2] &&
            prev->frag.upper[3] == d->frag.upper[3] &&
            !memcmp(prev->vert.model,d->vert.model,sizeof(d->vert.model))) same = true;
        if (same && prev->firstindex+prev->numindices == d->firstindex &&
            (sizeof(index_t)>2 || base+(size_t)mesh->numvertexes <= 65536)) {
            for (i = 0; i < mesh->numindexes; ++i) gpu.cpuindices[gpu.numindices+i] += base;
            prev->numindices += d->numindices;
            prev->boundsvalid = prev->boundsvalid && d->boundsvalid;
            for (i = 0; i < 3; ++i) {
                prev->mins[i] = min(prev->mins[i], d->mins[i]);
                prev->maxs[i] = max(prev->maxs[i], d->maxs[i]);
            }
            --gpu.numdraws;
        }
    }
    gpu.numvertices += mesh->numvertexes;
    if (!cached) gpu.numstreamvertices += mesh->numvertexes;
    gpu.numindices += mesh->numindexes;
}

void GPU_DrawMeshList(shader_t *shader, int count, mesh_t **meshes, vbo_t *vbo, texnums_t *tex, unsigned flags)
{
    int i, pass;
    qboolean programmed;
    if (!shader || shader->flags & SHADER_NODRAW) return;
    if (shader->remapto) shader = shader->remapto;
    if (!tex) tex = shader->defaulttextures;
    if (!tex) return;
    programmed = shader->prog != NULL;
    for (pass = 0; pass < shader->numpasses; pass += max(1, shader->passes[pass].numMergedPasses)) {
        gpu_draw_t state;
        const gpu_draw_t *prepared = NULL;
        qboolean cacheable = gpu.batching && !collectshadowonly &&
            !r_shadow_realtime_world.ival && !r_shadow_realtime_dlight.ival;
        for (i = 0; i < count; ++i) {
            size_t before = gpu.numvertices;
            GPU_DrawMesh(shader, meshes[i], vbo, tex, flags, &shader->passes[pass], prepared);
            if (cacheable && !prepared && gpu.numvertices != before) {
                state = gpu.draws[gpu.numdraws-1];
                prepared = &state;
            }
        }
        if (programmed) break;
    }
}
void GPU_DrawMeshSingle(shader_t *shader, mesh_t *mesh, vbo_t *vbo, unsigned flags)
{ GPU_DrawMeshList(shader, 1, &mesh, vbo, NULL, flags); }

static gpu_water_view_t *GPU_WaterView(batch_t *batch, mesh_t *mesh)
{
    extern cvar_t r_refractreflect_scale;
    vec3_t normal, point, center = {0,0,0};
    vec4_t plane;
    refdef_t saved;
    vec3_t forward, right, up, origin;
    SDL_GPUTexture *target = gpu.target, *depth = gpu.targetdepth;
    SDL_GPUTextureFormat format = gpu.targetformat;
    unsigned width = gpu.targetwidth, height = gpu.targetheight, i, j;
    int viewarea = r_viewarea, cluster = r_viewcluster, cluster2 = r_viewcluster2;
    entity_t *savedentity = entity;
    gpu_water_view_t *view;
    float scale = batch->ent->scale ? batch->ent->scale : 1;
    unsigned flags = batch->shader->flags & (SHADER_HASREFLECT|SHADER_HASREFRACT);
    if (!mesh->xyz_array || !mesh->normals_array || !mesh->numvertexes) return NULL;
    VectorCopy(mesh->normals_array[0], normal);
    for (i = 0; i < 3; ++i) {
        plane[i] = normal[0]*batch->ent->axis[0][i]+normal[1]*batch->ent->axis[1][i]+normal[2]*batch->ent->axis[2][i];
        point[i] = batch->ent->origin[i] + (mesh->xyz_array[0][0]*batch->ent->axis[0][i]+
            mesh->xyz_array[0][1]*batch->ent->axis[1][i]+mesh->xyz_array[0][2]*batch->ent->axis[2][i])*scale;
    }
    VectorNormalize(plane); plane[3] = DotProduct(point, plane);
    /* Orient the clipping plane towards the viewer, including underwater views. */
    if (DotProduct(r_refdef.vieworg,plane) < plane[3]) for (i = 0; i < 4; ++i) plane[i] = -plane[i];
    for (i = 0; i < numwaterviews; ++i) {
        view = &waterviews[i];
        if (view->flags == flags && DotProduct(view->plane,plane) > 0.9999f && fabs(view->plane[3]-plane[3]) < 0.1f)
            return view;
    }
    if (numwaterviews == GPU_MAX_WATER_VIEWS) return NULL;
    view = &waterviews[numwaterviews++];
    memcpy(view->plane, plane, sizeof(plane)); view->flags = flags;
    unsigned w = max(1, width*bound(0.125f,r_refractreflect_scale.value,1));
    unsigned h = max(1, height*bound(0.125f,r_refractreflect_scale.value,1));
    if (view->width != w || view->height != h || view->format != format) {
        SDL_GPUTextureCreateInfo ci = {0};
        ci.type = SDL_GPU_TEXTURETYPE_2D; ci.width = w; ci.height = h;
        ci.layer_count_or_depth = ci.num_levels = 1; ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        for (j = 0; j < 2; ++j) {
            if (view->color[j]) SDL_ReleaseGPUTexture(gpu.device, view->color[j]);
            if (view->depth[j]) SDL_ReleaseGPUTexture(gpu.device, view->depth[j]);
            ci.format = format; ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            view->color[j] = SDL_CreateGPUTexture(gpu.device, &ci);
            ci.format = gpu.depthformat; ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            view->depth[j] = SDL_CreateGPUTexture(gpu.device, &ci);
            if (!view->color[j] || !view->depth[j]) GPU_Error("create water view");
        }
        view->width = w; view->height = h; view->format = format;
    }
    saved = r_refdef;
    Matrix4_Multiply(saved.m_projection_std, saved.m_view, view->screenmatrix);
    VectorCopy(vpn, forward); VectorCopy(vright,right); VectorCopy(vup,up); VectorCopy(r_origin,origin);
    for (i = 0; i < mesh->numvertexes; ++i) VectorAdd(center,mesh->xyz_array[i],center);
    VectorScale(center,1.0f/mesh->numvertexes,center);
    for (i = 0; i < 3; ++i) point[i] = batch->ent->origin[i] + scale*(
        center[0]*batch->ent->axis[0][i]+center[1]*batch->ent->axis[1][i]+center[2]*batch->ent->axis[2][i]);
    for (j = 0; j < 2; ++j) {
        pvsbuffer_t visibility = {0};
        view->draws[j] = 0;
        if (!(flags & (j ? SHADER_HASREFRACT : SHADER_HASREFLECT))) continue;
        size_t firstdraw = gpu.numdraws;
        r_refdef = saved; r_refdef.recurse++; r_refdef.areabitsknown = false;
        VectorCopy(forward,vpn); VectorCopy(right,vright); VectorCopy(up,vup);
        if (!j) {
            float distance = DotProduct(r_refdef.vieworg,plane)-plane[3];
            VectorMA(r_refdef.vieworg,-2*distance,plane,r_refdef.vieworg);
            VectorMA(vpn,-2*DotProduct(vpn,plane),plane,vpn);
            VectorMA(vright,-2*DotProduct(vright,plane),plane,vright);
            VectorMA(vup,-2*DotProduct(vup,plane),plane,vup);
            r_refdef.flipcull ^= SHADER_CULL_FLIP;
            r_refdef.externalview = true;
        }
        VectorMA(point,j ? -0.5f : 0.5f,plane,r_refdef.pvsorigin);
        if (cl.worldmodel) {
            /* Original opaque-water VIS can omit the far side of a pool, and
               reflected views can see outside the viewer's PVS. Keep subview
               visibility conservative; frustum, backface, depth and the water
               plane still cull geometry. This also handles disconnected pools. */
            visibility.buffer = BZ_Malloc(visibility.buffersize = cl.worldmodel->pvsbytes);
            memset(visibility.buffer, 0xff, visibility.buffersize);
            r_refdef.forcevis = true; r_refdef.forcedvis = visibility.buffer;
        }
        VectorCopy(r_refdef.vieworg,r_origin);
        VectorAngles(vpn,vup,r_refdef.viewangles,false);
        Matrix4x4_CM_ModelViewMatrixFromAxis(r_refdef.m_view,vpn,vright,vup,r_refdef.vieworg);
        r_refdef.pxrect.x = saved.pxrect.x*w/width; r_refdef.pxrect.y = saved.pxrect.y*h/height;
        r_refdef.pxrect.width = saved.pxrect.width*w/width; r_refdef.pxrect.height = saved.pxrect.height*h/height;
        gpu.target = view->color[j]; gpu.targetdepth = view->depth[j];
        gpu.targetwidth = w; gpu.targetheight = h;
        memcpy(clipplane,plane,sizeof(plane));
        if (j) for (i = 0; i < 4; ++i) clipplane[i] = -clipplane[i];
        clipplane[3] += 0.01f; clipenabled = true;
        Surf_SetupFrame();
        R_SetFrustum(r_refdef.m_projection_std,r_refdef.m_view);
        Surf_DrawWorld();
        view->draws[j] = gpu.numdraws-firstdraw;
        BZ_Free(visibility.buffer);
    }
    clipenabled = false;
    r_refdef = saved; r_viewarea = viewarea; r_viewcluster = cluster; r_viewcluster2 = cluster2;
    VectorCopy(forward,vpn); VectorCopy(right,vright); VectorCopy(up,vup); VectorCopy(origin,r_origin);
    gpu.target = target; gpu.targetdepth = depth; gpu.targetformat = format;
    gpu.targetwidth = width; gpu.targetheight = height; entity = savedentity;
    R_SetFrustum(r_refdef.m_projection_std,r_refdef.m_view);
    return view;
}

void GPU_SubmitBatch(batch_t *b)
{
    if (b->buildmeshes) b->buildmeshes(b);
    if (b->meshes <= b->firstmesh) return;
    if (b->shader && (b->shader->flags & (SHADER_HASREFLECT|SHADER_HASREFRACT))) {
        if (r_refdef.recurse) return; /* A water view excludes recursive water surfaces. */
        int first = b->firstmesh, end = b->meshes, i;
        for (i = first; i < end; ++i) {
            /* The same water material can occur at different elevations. */
            currentwater = GPU_WaterView(b, b->mesh[i]);
            GPU_SelectEntity(b->ent); batchlightmap = b->lightmap[0];
            GPU_DrawMeshList(b->shader,1,&b->mesh[i],b->vbo,b->skin,b->flags);
            currentwater = NULL; batchlightmap = -1;
        }
        return;
    }
    GPU_SelectEntity(b->ent);
    batchlightmap = b->lightmap[0];
    GPU_DrawMeshList(b->shader, b->meshes-b->firstmesh, b->mesh+b->firstmesh, b->vbo, b->skin, b->flags);
    batchlightmap = -1;
    currentwater = NULL;
}

batch_t *GPU_GetTempBatch(void)
{
    if (numtempbatches == maxtempbatches) {
        maxtempbatches++;
        tempbatches = BZ_Realloc(tempbatches, maxtempbatches*sizeof(*tempbatches));
        tempbatches[maxtempbatches-1] = Z_Malloc(sizeof(batch_t));
    }
    batch_t *b = tempbatches[numtempbatches++]; memset(b, 0, sizeof(*b)); return b;
}

void GPU_DrawWorld(batch_t **world)
{
    batch_t *models[SHADER_SORT_COUNT], *b;
    unsigned sort;
    BE_GenModelBatches(models, NULL, BEM_STANDARD, r_refdef.scenevis, r_refdef.sceneareas);
    GPU_UploadLightmaps(false);
    mode = BEM_STANDARD;
    Vector4Set(r_worldentity.shaderRGBAf, 1,1,1,1);
    for (sort = 0; sort < SHADER_SORT_COUNT; ++sort) {
        for (b = world ? world[sort] : NULL; b; b = b->next) {
            if (b->shader && b->shader->flags & SHADER_SKY && R_DrawSkyChain(b)) continue;
            GPU_SubmitBatch(b);
        }
        for (b = models[sort]; b; b = b->next) GPU_SubmitBatch(b);
    }
    /* All BSP surfaces cast shadows, including surfaces outside the camera frustum. */
    if (!r_refdef.recurse && numshadowlights && r_worldentity.model) {
        model_t *model = r_worldentity.model;
        unsigned i;
        collectshadowonly = true;
        GPU_SelectEntity(&r_worldentity);
        unsigned sort;
        for (sort = 0; sort < SHADER_SORT_COUNT; ++sort) {
            batch_t *batch;
            for (batch = model->batches[sort]; batch; batch = batch->next) {
                if (!batch->shader || batch->shader->flags & (SHADER_SKY|SHADER_NOSHADOWS|SHADER_NODRAW)) continue;
                for (i = 0; i < model->nummodelsurfaces; ++i) {
                    msurface_t *surface = &model->surfaces[model->firstmodelsurface+i];
                    if (!surface->mesh || surface->sbatch != batch) continue;
                    GPU_DrawMeshSingle(batch->shader, surface->mesh, batch->vbo, batch->flags);
                }
            }
        }
        collectshadowonly = false;
    }
    R_RenderDlights();
    GPU_SelectEntity(&r_worldentity);
    if (r_refdef.recurse) RQ_RenderBatch(); else RQ_RenderBatchClear();
}
void GPU_GenBrushModelVBO(model_t *model)
{
    /* Rebuilt lightmap coordinates or a reloaded map invalidate cached data.
     * Retire pages at the next frame boundary, after pending draws submit. */
    staticinvalid = true;
    model->vbos = NULL;
}
void GPU_ClearVBO(vbo_t *vbo, qboolean dataonly)
{
    if (!vbo) return;
    Z_Free(vbo->vbomem); Z_Free(vbo->ebomem);
    if (!dataonly) Z_Free(vbo);
}
void GPU_UploadAllLightmaps(void) { GPU_UploadLightmaps(true); }
qboolean GPU_SelectDLight(dlight_t *dl, vec3_t color, vec3_t axis[3], unsigned lmode)
{ return lmode == 0; }
qboolean GPU_LightCullModel(vec3_t org, model_t *model) { return false; /* Generic entity batching already performs camera culling. */ }
void GPU_VBOBegin(vbobctx_t *ctx, size_t size)
{ memset(ctx, 0, sizeof(*ctx)); ctx->maxsize = size; ctx->fallback = BZ_Malloc(size); }
void GPU_VBOData(vbobctx_t *ctx, void *data, size_t size, vboarray_t *array)
{
    if (size > ctx->maxsize-ctx->pos) Sys_Error("SDL GPU: VBO staging overflow");
    array->sysptr = (char *)ctx->fallback+ctx->pos;
    memcpy(array->sysptr, data, size); ctx->pos += size;
}
void GPU_VBOFinish(vbobctx_t *ctx, void *data, size_t size, vboarray_t *array, void **vertices, void **indices)
{
    *indices = array->sysptr = BZ_Malloc(size); memcpy(*indices, data, size);
    if (ctx) { *vertices = ctx->fallback; ctx->fallback = NULL; }
}
void GPU_VBODestroy(vboarray_t *array, void *memory) { Z_Free(memory); array->sysptr = NULL; }
void GPU_RenderToTextureUpdate2D(qboolean changed)
{
    if (!changed) return;
    if (R2D_Flush) R2D_Flush();
    gpu.target = gpu.scene; gpu.targetdepth = gpu.depth; gpu.targetformat = gpu.sceneformat;
    gpu.targetwidth = gpu.width; gpu.targetheight = gpu.height;
    if (*r_refdef.rt_destcolour[0].texname) {
        unsigned w, h;
        texid_t tex = R2D_RT_GetTexture(r_refdef.rt_destcolour[0].texname, &w, &h);
        if (tex && tex->gpuimage && w && h) {
            gpu_target_t *t;
            SDL_GPUTextureCreateInfo ci = {0};
            for (t = targets; t; t = t->next) if (t->image == tex) break;
            if (!t) { t = Z_Malloc(sizeof(*t)); t->image = tex; t->next = targets; targets = t; }
            if (!t->depth || t->width != w || t->height != h) {
                if (t->depth) SDL_ReleaseGPUTexture(gpu.device, t->depth);
                ci.type = SDL_GPU_TEXTURETYPE_2D; ci.format = gpu.depthformat;
                ci.width = w; ci.height = h; ci.layer_count_or_depth = ci.num_levels = 1;
                ci.sample_count = SDL_GPU_SAMPLECOUNT_1; ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
                t->depth = SDL_CreateGPUTexture(gpu.device, &ci);
                if (!t->depth) GPU_Error("create offscreen depth target");
                t->width = w; t->height = h;
            }
            t->color = tex->gpuimage->texture;
            gpu.target = t->color; gpu.targetdepth = t->depth; gpu.targetformat = tex->gpuimage->format;
            gpu.targetwidth = w; gpu.targetheight = h;
        }
    }
    GPU_Set2D();
}
