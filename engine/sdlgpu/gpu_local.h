/* FTE SDL3 GPU renderer. GPL-2.0-or-later, like the engine. */
#ifndef FTE_GPU_LOCAL_H
#define FTE_GPU_LOCAL_H
#include "quakedef.h"
#include "glquake.h"
#include "gl_draw.h"
#include "shader.h"
#include <SDL3/SDL.h>

#undef VID_SwapBuffers
#undef VID_MayRefresh
#undef BE_SelectMode
#undef BE_DrawMesh_List
#undef BE_DrawMesh_Single
#undef BE_SubmitBatch
#undef BE_GetTempBatch
#undef BE_DrawWorld
#undef BE_Init
#undef BE_GenBrushModelVBO
#undef BE_ClearVBO
#undef BE_UploadAllLightmaps
#undef BE_SelectEntity
#undef BE_SelectDLight
#undef BE_Scissor
#undef BE_LightCullModel
#undef BE_VBO_Begin
#undef BE_VBO_Data
#undef BE_VBO_Finish
#undef BE_VBO_Destroy
#undef BE_RenderToTextureUpdate2d
extern qboolean vid_isfullscreen;
void R2D_Console_Resize(void);
void R_FetchPlayerColour(unsigned int, vec3_t);
#define GPU_SAMPLERS 12
#define GPU_MAX_SHADOWS 8
#define GPU_SHADOW_SIZE 512
#define GPU_MAX_LIGHTS 32

typedef struct gpu_image_s {
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
    SDL_GPUTextureFormat format;
    SDL_GPUTextureType type;
    SDL_GPUTextureUsageFlags usage;
    unsigned width, height, layers, levels;
} gpu_image_t;

typedef struct {
    float position[3], uv[2], lmuv[2], color[4];
    float normal[3], tangent[3], bitangent[3];
} gpu_vertex_t;

typedef struct {
    float mvp[16], model[16];
} gpu_vertex_uniforms_t;

typedef struct {
    float eye[4], fog[4], effects[4], material[4];
    float ambient[4], diffuse[4], lightdir[4];
    float upper[4], lower[4], settings[4]; /* upper.w holds the program's alpha cutoff. */
    float clipplane[4], screenmatrix[16];
    float lightpos[GPU_MAX_LIGHTS][4], lightcolor[GPU_MAX_LIGHTS][4], lightshadow[GPU_MAX_LIGHTS][4], lightparams[GPU_MAX_LIGHTS][4];
} gpu_fragment_uniforms_t;

typedef struct gpu_draw_s {
    unsigned firstindex, numindices, vertexoffset, bits, cull;
    qboolean lines, castshadow, shadowonly;
    SDL_GPUTexture *target, *depth;
    SDL_GPUTextureFormat format;
    SDL_GPUViewport viewport;
    SDL_Rect scissor;
    SDL_GPUTextureSamplerBinding samplers[GPU_SAMPLERS];
    gpu_vertex_uniforms_t vert;
    gpu_fragment_uniforms_t frag;
} gpu_draw_t;

typedef struct {
    SDL_GPUDevice *device;
    SDL_GPUCommandBuffer *command;
    SDL_GPUTexture *swapchain, *depth, *scene, *present, *shadowmap, *fallbackcube;
    SDL_GPUTextureFormat colorformat, depthformat, sceneformat, targetformat;
    SDL_GPUTexture *target, *targetdepth;
    unsigned targetwidth, targetheight;
    SDL_GPUShader *vertexshader, *fragmentshader, *postvertex, *postfragment, *shadowvertex, *shadowfragment;
    SDL_GPUGraphicsPipeline *postpipeline, *shadowpipeline;
    SDL_GPUSampler *postsampler, *shadowsampler;
    SDL_GPUBuffer *vertices, *indices;
    SDL_GPUTransferBuffer *transfer;
    size_t vertexcapacity, indexcapacity, transfercapacity;
    gpu_vertex_t *cpuvertices;
    index_t *cpuindices;
    gpu_draw_t *draws;
    size_t numvertices, numindices, numdraws, maxvertices, maxindices, maxdraws;
    unsigned width, height;
    qboolean drawing2d;
    qboolean benchmarking;
    qboolean shadowinitialized;
    qboolean batching;
    double framebegin, frameacquired;
} gpu_state_t;
extern gpu_state_t gpu;
extern SDL_Window *sdlwindow;
extern qboolean mouseactive;
extern cvar_t r_bloom;
extern cvar_t r_normalmapping;
void GPU_Error(const char *operation);
SDL_GPUTextureFormat GPU_TextureFormat(uploadfmt_t fmt);
void GPU_InitTextureFormats(void);
void GPU_UpdateFiltering(image_t *, int[3], int[3], int[2], float, float);
qboolean GPU_LoadTextureMips(texid_t, const struct pendingtextureinfo *);
void GPU_DestroyTexture(texid_t);
SDL_GPUTextureSamplerBinding GPU_TextureBinding(texid_t tex, texid_t fallback);
void GPU_UploadLightmaps(qboolean force);
void GPU_BackendInit(void);
void GPU_BackendShutdown(void);
void GPU_BeginFrame(void);
void GPU_CheckLightingSettings(void);
void GPU_EndFrame(void);
void GPU_WaterInfo(void);
void GPU_BenchmarkFrame(double encodebegin, double submitted);
void GPU_Set2D(void);
void GPU_SelectMode(backendmode_t);
void GPU_DrawMeshList(shader_t *, int, mesh_t **, vbo_t *, texnums_t *, unsigned);
void GPU_DrawMeshSingle(shader_t *, mesh_t *, vbo_t *, unsigned);
void GPU_SubmitBatch(batch_t *);
batch_t *GPU_GetTempBatch(void);
void GPU_DrawWorld(batch_t **);
void GPU_GenBrushModelVBO(model_t *);
void GPU_ClearVBO(vbo_t *, qboolean);
void GPU_UploadAllLightmaps(void);
void GPU_SelectEntity(entity_t *);
qboolean GPU_SelectDLight(dlight_t *, vec3_t, vec3_t[3], unsigned);
void GPU_Scissor(srect_t *);
qboolean GPU_LightCullModel(vec3_t, model_t *);
void GPU_VBOBegin(vbobctx_t *, size_t);
void GPU_VBOData(vbobctx_t *, void *, size_t, vboarray_t *);
void GPU_VBOFinish(vbobctx_t *, void *, size_t, vboarray_t *, void **, void **);
void GPU_VBODestroy(vboarray_t *, void *);
void GPU_RenderToTextureUpdate2D(qboolean);
#endif
