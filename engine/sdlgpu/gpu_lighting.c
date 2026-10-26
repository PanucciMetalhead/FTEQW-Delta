/* CPU-binned clustered forward lighting using SDL GPU storage buffers.
 * GPL-2.0-or-later. Light assignment is shared by all meshes in a view. */
#include "gpu_local.h"
#include "gpu_cluster.h"

typedef struct {
    float position[4], color[4], shadow[4], params[4];
} gpu_light_t;
typedef struct { unsigned offset, count; } gpu_cell_t;
typedef struct { int lo[3], hi[3]; } gpu_light_range_t;
struct gpu_lightview_s {
    float view[16], projection[16];
    gpu_light_t *lights;
    gpu_light_range_t *ranges;
    unsigned numlights, maxlights, numindices, maxindices, overflows, sceneid;
    unsigned *indices;
    gpu_cell_t cells[GPU_CLUSTER_COUNT];
    SDL_GPUBuffer *buffers[3];
    unsigned capacities[3], transfercapacity;
    SDL_GPUTransferBuffer *transfer;
    qboolean used;
    struct gpu_lightview_s *next;
};
static gpu_lightview_t *lightviews, *currentview;
static SDL_GPUBuffer *emptybuffer;
static unsigned lightsceneid;
static cvar_t gpu_clustered = CVARD("gpu_clustered", "1", "Lighting: 0 reference (32 lights per mesh), 1 clustered forward, 2 full per-view list for validation. Overflowing clusters evaluate all visible lights.");

void GPU_LightingInit(void)
{
    SDL_GPUBufferCreateInfo bi = {0};
    if (Cvar_FindVar(gpu_clustered.name) != &gpu_clustered) Cvar_Register(&gpu_clustered, "SDL GPU");
    bi.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ; bi.size = sizeof(gpu_light_t);
    emptybuffer = SDL_CreateGPUBuffer(gpu.device, &bi);
    if (!emptybuffer) GPU_Error("create empty lighting binding");
    /* No shader reads this buffer unless lightgrid.x is set. Bind a valid
     * resource to every declared slot, including UI and reference draws. */
}

void GPU_LightingShutdown(void)
{
    while (lightviews) {
        unsigned i;
        gpu_lightview_t *v = lightviews;
        lightviews = v->next;
        for (i = 0; i < 3; ++i) if (v->buffers[i]) SDL_ReleaseGPUBuffer(gpu.device, v->buffers[i]);
        if (v->transfer) SDL_ReleaseGPUTransferBuffer(gpu.device, v->transfer);
        Z_Free(v->lights); Z_Free(v->ranges); Z_Free(v->indices); Z_Free(v);
    }
    if (emptybuffer) SDL_ReleaseGPUBuffer(gpu.device, emptybuffer);
    emptybuffer = NULL; currentview = NULL;
}

void GPU_LightingBeginFrame(void)
{
    gpu_lightview_t *v;
    for (v = lightviews; v; v = v->next) v->used = false;
    currentview = NULL; lightsceneid = 0;
}

void GPU_LightingNewScene(void)
{
    /* CSQC can redraw the same camera with a different set of transient
     * lights in one frame. Preserve the earlier scene's immutable light list. */
    ++lightsceneid; currentview = NULL;
}

static void GPU_BinLights(gpu_lightview_t *v)
{
    unsigned i, cursor[GPU_CLUSTER_COUNT];
    int x, y, z;
    memset(v->cells, 0, sizeof(v->cells));
    for (i = 0; i < v->numlights; ++i) {
        gpu_light_range_t *r = &v->ranges[i];
        float center[3];
        unsigned axis;
        for (axis = 0; axis < 3; ++axis)
            center[axis] = v->view[axis]*v->lights[i].position[0]+v->view[4+axis]*v->lights[i].position[1]+
                v->view[8+axis]*v->lights[i].position[2]+v->view[12+axis];
        GPU_ClusterRange(center, v->lights[i].position[3], v->projection, r->lo, r->hi);
        for (z = r->lo[2]; z <= r->hi[2]; ++z) for (y = r->lo[1]; y <= r->hi[1]; ++y)
            for (x = r->lo[0]; x <= r->hi[0]; ++x) ++v->cells[(z*GPU_CLUSTER_Y+y)*GPU_CLUSTER_X+x].count;
    }
    v->numindices = v->overflows = 0;
    for (i = 0; i < GPU_CLUSTER_COUNT; ++i) {
        gpu_cell_t *cell = &v->cells[i];
        cursor[i] = cell->offset = v->numindices;
        if (cell->count > GPU_CLUSTER_LIMIT) { cell->count = ~0u; ++v->overflows; }
        else v->numindices += cell->count;
    }
    if (max(1,v->numindices) > v->maxindices) {
        v->maxindices = max(1,v->numindices);
        v->indices = BZ_Realloc(v->indices,v->maxindices*sizeof(*v->indices));
    }
    if (!v->numindices) v->indices[0] = 0;
    for (i = 0; i < v->numlights; ++i) {
        const gpu_light_range_t *r = &v->ranges[i];
        for (z = r->lo[2]; z <= r->hi[2]; ++z) for (y = r->lo[1]; y <= r->hi[1]; ++y)
            for (x = r->lo[0]; x <= r->hi[0]; ++x) {
                unsigned cell = (z*GPU_CLUSTER_Y+y)*GPU_CLUSTER_X+x;
                if (v->cells[cell].count != ~0u) v->indices[cursor[cell]++] = i;
            }
    }
}

static void GPU_BuildLightView(gpu_lightview_t *v)
{
    unsigned i, selected[GPU_MAX_SHADOWS], numselected = 0;
    float scores[GPU_MAX_SHADOWS];
    dlight_t *shadowcandidates[GPU_MAX_SHADOWS];
    v->numlights = 0;
    if (v->maxlights < rtlights_max) {
        v->maxlights = rtlights_max;
        v->lights = BZ_Realloc(v->lights,v->maxlights*sizeof(*v->lights));
        v->ranges = BZ_Realloc(v->ranges,v->maxlights*sizeof(*v->ranges));
    }
    for (i = 0; i < rtlights_max; ++i) {
        dlight_t *l = &cl_dlights[i];
        gpu_light_t *out;
        float style = 1, score;
        vec3_t delta;
        unsigned slot, j;
        qboolean world = i >= RTL_FIRST;
        if (l->radius <= 0 || (world ? !r_shadow_realtime_world.ival : !r_shadow_realtime_dlight.ival) ||
            !(l->flags & (world ? LFLAG_REALTIMEMODE : LFLAG_NORMALMODE)) || R_CullSphere(l->origin,l->radius)) continue;
        if (l->style >= 0 && l->style < MAX_NET_LIGHTSTYLES) style = d_lightstylevalue[l->style]/256.0f;
        if (style <= 0 || !(fabs(l->color[0])+fabs(l->color[1])+fabs(l->color[2]) > 0)) continue;
        out = &v->lights[v->numlights]; memset(out,0,sizeof(*out));
        VectorCopy(l->origin,out->position); out->position[3] = l->radius;
        VectorScale(l->color,style,out->color); VectorCopy(l->lightcolourscales,out->params);
        VectorSubtract(l->origin,r_refdef.vieworg,delta);
        score = style*(fabs(l->color[0])+fabs(l->color[1])+fabs(l->color[2]))/
            (1+DotProduct(delta,delta)/(l->radius*l->radius));
        /* Rank shadow candidates once for the view. Non-shadowing lights must
         * not displace real shadow candidates from the small map budget. */
        extern cvar_t r_shadow_realtime_world_shadows, r_shadow_realtime_dlight_shadows;
        if (!(l->flags & LFLAG_NOSHADOWS) && l->radius > 2 &&
            (world ? r_shadow_realtime_world_shadows.ival : r_shadow_realtime_dlight_shadows.ival)) {
            for (slot = 0; slot < numselected && scores[slot] >= score; ++slot) {}
            if (slot < GPU_MAX_SHADOWS) {
                if (numselected < GPU_MAX_SHADOWS) ++numselected;
                for (j = numselected-1; j > slot; --j) {
                    scores[j] = scores[j-1]; selected[j] = selected[j-1]; shadowcandidates[j] = shadowcandidates[j-1];
                }
                scores[slot] = score; selected[slot] = v->numlights; shadowcandidates[slot] = l;
            }
        }
        ++v->numlights;
    }
    for (i = 0; i < numselected; ++i) {
        gpu_light_t *out = &v->lights[selected[i]];
        out->color[3] = GPU_ShadowSlot(shadowcandidates[i],out->shadow);
    }
    if (v->numlights) GPU_BinLights(v);
}

qboolean GPU_ClusterLights(gpu_draw_t *d)
{
    gpu_lightview_t *v, *available = NULL;
    unsigned i;
    if (!gpu_clustered.ival) return false;
    v = currentview;
    if (!v || memcmp(v->view,r_refdef.m_view,sizeof(v->view)) ||
        memcmp(v->projection,r_refdef.m_projection_std,sizeof(v->projection))) {
        for (v = lightviews; v; v = v->next) {
            if (!v->used) { if (!available) available = v; continue; }
            if (v->sceneid == lightsceneid && !memcmp(v->view,r_refdef.m_view,sizeof(v->view)) &&
                !memcmp(v->projection,r_refdef.m_projection_std,sizeof(v->projection))) break;
        }
        if (!v) {
            v = available;
            if (!v) { v = Z_Malloc(sizeof(*v)); v->next = lightviews; lightviews = v; }
            memcpy(v->view,r_refdef.m_view,sizeof(v->view));
            memcpy(v->projection,r_refdef.m_projection_std,sizeof(v->projection));
            v->used = true; v->sceneid = lightsceneid; GPU_BuildLightView(v);
        }
        currentview = v;
    }
    if (!v->numlights) return true;
    d->lighting = v; d->frag.material[3] = v->numlights;
    Matrix4_Multiply(v->projection,v->view,d->frag.lightmatrix);
    for (i = 0; i < 4; ++i) d->frag.lightdepth[i] = -v->view[i*4+2];
    d->frag.lightgrid[0] = gpu_clustered.ival == 2 ? 2 : 1; d->frag.lightgrid[1] = GPU_CLUSTER_NEAR;
    d->frag.lightgrid[2] = GPU_CLUSTER_FAR;
    d->frag.lightgrid[3] = GPU_CLUSTER_Z/logf(GPU_CLUSTER_FAR/GPU_CLUSTER_NEAR);
    return true;
}

void GPU_LightingUpload(void)
{
    gpu_lightview_t *v;
    for (v = lightviews; v; v = v->next) if (v->used && v->numlights) {
        unsigned sizes[3] = {v->numlights*sizeof(*v->lights),sizeof(v->cells),max(1,v->numindices)*sizeof(*v->indices)};
        const void *data[3] = {v->lights,v->cells,v->indices};
        unsigned total = sizes[0]+sizes[1]+sizes[2], i, offset = 0;
        qbyte *map;
        SDL_GPUCopyPass *pass;
        if (v->transfercapacity < total) {
            SDL_GPUTransferBufferCreateInfo ti = {0};
            if (v->transfer) SDL_ReleaseGPUTransferBuffer(gpu.device,v->transfer);
            ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; ti.size = v->transfercapacity = total;
            v->transfer = SDL_CreateGPUTransferBuffer(gpu.device,&ti);
            if (!v->transfer) GPU_Error("create cluster upload buffer");
        }
        map = SDL_MapGPUTransferBuffer(gpu.device,v->transfer,true);
        if (!map) GPU_Error("map clustered lights");
        for (i = 0; i < 3; ++i) { memcpy(map+offset,data[i],sizes[i]); offset += sizes[i]; }
        SDL_UnmapGPUTransferBuffer(gpu.device,v->transfer);
        pass = SDL_BeginGPUCopyPass(gpu.command); offset = 0;
        for (i = 0; i < 3; ++i) {
            SDL_GPUTransferBufferLocation src = {v->transfer,offset};
            SDL_GPUBufferRegion dst = {0};
            if (v->capacities[i] < sizes[i]) {
                SDL_GPUBufferCreateInfo bi = {0};
                if (v->buffers[i]) SDL_ReleaseGPUBuffer(gpu.device,v->buffers[i]);
                bi.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ; bi.size = v->capacities[i] = sizes[i];
                v->buffers[i] = SDL_CreateGPUBuffer(gpu.device,&bi);
                if (!v->buffers[i]) GPU_Error("create clustered light storage");
            }
            dst.buffer = v->buffers[i]; dst.size = sizes[i];
            SDL_UploadToGPUBuffer(pass,&src,&dst,true); offset += sizes[i];
        }
        SDL_EndGPUCopyPass(pass);
    }
}

void GPU_LightingBind(SDL_GPURenderPass *pass, const gpu_draw_t *d)
{
    SDL_GPUBuffer *buffers[3] = {emptybuffer,emptybuffer,emptybuffer};
    if (d->lighting) memcpy(buffers,d->lighting->buffers,sizeof(buffers));
    SDL_BindGPUFragmentStorageBuffers(pass,0,buffers,3);
}

void GPU_LightingInfo(void)
{
    gpu_lightview_t *v;
    unsigned views = 0, lights = 0, links = 0, overflows = 0;
    for (v = lightviews; v; v = v->next) if (v->used && v->numlights) {
        ++views; lights += v->numlights; links += v->numindices; overflows += v->overflows;
    }
    Con_Printf("Clustered lighting: %u views, %u lights, %u indices, %u overflow cells (full-list fallback)\n",views,lights,links,overflows);
}
