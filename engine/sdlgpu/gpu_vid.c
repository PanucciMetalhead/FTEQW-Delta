/* SDL3 window and frame integration. GPL-2.0-or-later. */
#include "gpu_local.h"
#include "renderque.h"

gpu_state_t gpu;
SDL_Window *sdlwindow;
cvar_t r_normalmapping = CVARFD("r_normalmapping", "1", CVAR_ARCHIVE, "Enable normal maps on lit materials. Changes apply immediately.");
cvar_t r_bloom = CVARAFD("r_bloom", "0", "gl_bloom", CVAR_ARCHIVE, "Amount of bloom around bright surfaces; zero disables bloom.");

static struct {
    double until, previous, intervals[16384], acquire, build, encode, elapsed;
    size_t frames;
    double draws, vertices;
    char label[64];
} benchmark;

static void GPU_Benchmark_f(void)
{
    memset(&benchmark, 0, sizeof(benchmark));
    Q_strncpyz(benchmark.label, Cmd_Argc() > 2 ? Cmd_Argv(2) : "scene", sizeof(benchmark.label));
    benchmark.until = Sys_DoubleTime() + bound(1, Cmd_Argc() > 1 ? atof(Cmd_Argv(1)) : 10, 60);
    gpu.benchmarking = true;
    Con_Printf("SDL GPU benchmark started: %s (disable vid_vsync and raise cl_maxfps for throughput measurements)\n", benchmark.label);
}

static void GPU_Batching_f(void)
{
    if (Cmd_Argc() > 1) gpu.batching = atoi(Cmd_Argv(1)) != 0;
    Con_Printf("SDL GPU submission batching: %s\n", gpu.batching ? "enabled" : "disabled");
}

static int GPU_CompareTime(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

void GPU_BenchmarkFrame(double encodebegin, double submitted)
{
    if (benchmark.previous) {
        double interval = gpu.framebegin - benchmark.previous;
        benchmark.intervals[benchmark.frames % countof(benchmark.intervals)] = interval;
        benchmark.elapsed += interval;
        benchmark.acquire += gpu.frameacquired - gpu.framebegin;
        benchmark.build += encodebegin - gpu.frameacquired;
        benchmark.encode += submitted - encodebegin;
        benchmark.draws += gpu.numdraws; benchmark.vertices += gpu.numvertices;
        ++benchmark.frames;
    }
    benchmark.previous = gpu.framebegin;
    if (submitted >= benchmark.until && benchmark.frames) {
        size_t n = min(benchmark.frames, countof(benchmark.intervals));
        double scale = 1000.0 / benchmark.frames;
        qsort(benchmark.intervals, n, sizeof(benchmark.intervals[0]), GPU_CompareTime);
        Con_Printf("SDL GPU benchmark %s %ux%u: %.2f fps, %.3f ms/frame, p50 %.3f ms, p95 %.3f ms; acquire %.3f ms, build %.3f ms, encode %.3f ms; %.0f draws, %.0f vertices/frame (%u frames)\n",
            benchmark.label, gpu.width, gpu.height, benchmark.frames / benchmark.elapsed, benchmark.elapsed * scale,
            benchmark.intervals[n/2]*1000, benchmark.intervals[(n-1)*95/100]*1000,
            benchmark.acquire * scale, benchmark.build * scale, benchmark.encode * scale,
            benchmark.draws / benchmark.frames, benchmark.vertices / benchmark.frames, (unsigned)benchmark.frames);
        gpu.benchmarking = false;
    }
}

void GPU_Error(const char *operation)
{
    Sys_Error("SDL3 GPU: %s: %s", operation, SDL_GetError());
}

static void GPU_Info_f(void)
{
    extern texid_t r_whiteimage, missing_texture;
    size_t i; unsigned material[7] = {0}, lights = 0, shadows = 0, loaded = 0, fallback = 0;
    unsigned cutouts = 0, missing = 0, cutoutshadows = 0;
    for (i = 0; i < gpu.numdraws; ++i) {
        gpu_draw_t *d = &gpu.draws[i];
        unsigned type = d->frag.material[0];
        if (type < countof(material) && !d->shadowonly) ++material[type];
        lights = max(lights, (unsigned)d->frag.material[3]);
        if (d->castshadow) ++shadows;
        if (d->frag.material[2] == 4 || d->frag.material[2] == 5) {
            if (d->castshadow) ++cutoutshadows;
            if (!d->shadowonly) {
                ++cutouts;
                if (d->samplers[0].texture == missing_texture->gpuimage->texture) ++missing;
            }
        }
    }
    Con_Printf("SDL GPU %s %ux%u: %u draws, %u vertices, %u indices; material draws %u/%u/%u/%u/%u/%u/%u, %u lights, %u shadow caster draws\n",
        SDL_GetGPUDeviceDriver(gpu.device), gpu.width, gpu.height,
        (unsigned)gpu.numdraws, (unsigned)gpu.numvertices, (unsigned)gpu.numindices,
        material[0],material[1],material[2],material[3],material[4],material[5],material[6],lights,shadows);
    for (i = 0; i < numlightmaps; ++i) {
        lightmapinfo_t *lm = lightmap[i];
        if (lm && lm->lightmap_texture && lm->lightmap_texture->status == TEX_LOADED) ++loaded;
    }
    for (i = 0; i < gpu.numdraws; ++i)
        if (!gpu.draws[i].shadowonly && !gpu.draws[i].vertexlit && gpu.draws[i].frag.material[0] == 1 &&
            gpu.draws[i].samplers[4].texture == r_whiteimage->gpuimage->texture) ++fallback;
    GPU_PaniniInfo();
    Con_Printf("Lightmaps: %u/%u loaded, %u draws using fallback\n",loaded,numlightmaps,fallback);
    Con_Printf("Cutouts: %u draws, %u using missing texture, %u shadow caster draws\n",cutouts,missing,cutoutshadows);
    GPU_WaterInfo();
}

static void GPU_VideoModes(const char *driver, const char *output, void (*cb)(int, int))
{
    int count, i;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &count);
    for (i = 0; modes && i < count; ++i) cb(modes[i]->w, modes[i]->h);
    SDL_free(modes);
}

static void GPU_VideoShutdown(void)
{
    Cmd_RemoveCommand("gpu_info");
    Cmd_RemoveCommand("gpu_benchmark");
    Cmd_RemoveCommand("gpu_batching");
    if (gpu.device) {
        SDL_WaitForGPUIdle(gpu.device);
        GPU_BackendShutdown();
        if (sdlwindow) SDL_ReleaseWindowFromGPUDevice(gpu.device, sdlwindow);
        SDL_DestroyGPUDevice(gpu.device);
    }
    if (sdlwindow) SDL_DestroyWindow(sdlwindow);
    sdlwindow = NULL;
    memset(&gpu, 0, sizeof(gpu));
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static qboolean GPU_VideoInit(rendererstate_t *info, unsigned char *palette)
{
    const char *driver;
    SDL_GPUPresentMode present;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_DisplayID display;
#ifdef __APPLE__
    driver = "metal";
#else
    driver = "vulkan";
#endif
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) goto fail;
    display = SDL_GetPrimaryDisplay();
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (info->devicename[0]) {
        int count, i;
        SDL_DisplayID *displays = SDL_GetDisplays(&count);
        for (i = 0; displays && i < count; ++i)
            if (!strcmp(info->devicename, SDL_GetDisplayName(displays[i]))) display = displays[i];
        SDL_free(displays);
    }
    sdlwindow = SDL_CreateWindow(FULLENGINENAME " — SDL3 GPU", info->width, info->height, flags);
    if (!sdlwindow) goto fail;
    SDL_SetWindowPosition(sdlwindow, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    SDL_SetWindowMinimumSize(sdlwindow, 320, 200);
    if (info->fullscreen) {
        if (info->fullscreen == 1) {
            SDL_DisplayMode mode;
            if (SDL_GetClosestFullscreenDisplayMode(display, info->width, info->height, info->rate, true, &mode))
                SDL_SetWindowFullscreenMode(sdlwindow, &mode);
        }
        if (!SDL_SetWindowFullscreen(sdlwindow, true)) goto fail;
    }
    gpu.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL,
        COM_CheckParm("-gpu_debug") != 0, driver);
    if (!gpu.device) goto fail;
    if (!SDL_ClaimWindowForGPUDevice(gpu.device, sdlwindow)) goto fail;
    /* Select explicitly: a Windows installation must not silently choose D3D12. */
    if (strcmp(SDL_GetGPUDeviceDriver(gpu.device), driver)) {
        SDL_SetError("requested %s, received %s", driver, SDL_GetGPUDeviceDriver(gpu.device)); goto fail;
    }
    present = info->wait == 0 ? SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_VSYNC;
    if (!SDL_WindowSupportsGPUPresentMode(gpu.device, sdlwindow, present)) present = SDL_GPU_PRESENTMODE_VSYNC;
    if (!SDL_SetGPUSwapchainParameters(gpu.device, sdlwindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present)) goto fail;
    gpu.colorformat = SDL_GetGPUSwapchainTextureFormat(gpu.device, sdlwindow);
    gpu.depthformat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    SDL_GetWindowSizeInPixels(sdlwindow, (int *)&vid.pixelwidth, (int *)&vid.pixelheight);
    vid.dpi_x = vid.dpi_y = SDL_GetWindowDisplayScale(sdlwindow)*96;
    vid.activeapp = true; vid.numpages = 2;
    vid_isfullscreen = info->fullscreen != 0;
    vid.flags = 0; vid.gammarampsize = 0;
    memset(&sh_config, 0, sizeof(sh_config));
    sh_config.progs_supported = true;
    sh_config.progs_required = true;
    sh_config.progpath = "sdlgpu/%s";
    sh_config.texture_non_power_of_two = true;
    GPU_InitTextureFormats();
    Cmd_AddCommandD("gpu_info", GPU_Info_f, "Report SDL GPU device, frame geometry, material, and light counts.");
    Cmd_AddCommandD("gpu_benchmark", GPU_Benchmark_f, "Measure frame throughput and CPU stages: gpu_benchmark [seconds] [label]. Warm up first; disable vsync and raise cl_maxfps for throughput.");
    Cmd_AddCommandD("gpu_batching", GPU_Batching_f, "Compare optimized/reference submission: gpu_batching 1/0. Enabled by default; resets on video restart. Does not change graphical features.");
    if (Cvar_FindVar(r_normalmapping.name) != &r_normalmapping) Cvar_Register(&r_normalmapping, "Graphical Nicaties");
    if (Cvar_FindVar(r_bloom.name) != &r_bloom) Cvar_Register(&r_bloom, "Graphical Nicaties");
    Con_Printf("SDL3 GPU renderer: %s\n", SDL_GetGPUDeviceDriver(gpu.device));
    Q_strncpyz(info->subrenderer, driver, sizeof(info->subrenderer));
    return true;
fail:
    Con_Printf(CON_ERROR "SDL3 GPU initialization failed: %s\n", SDL_GetError());
    GPU_VideoShutdown();
    return false;
}

static qboolean GPU_Gamma(unsigned int size, unsigned short *ramps) { return false; }
static void GPU_Caption(const char *text) { if (sdlwindow) SDL_SetWindowTitle(sdlwindow, text); }
static void *GPU_CreateCursor(const qbyte *pixels, int w, int h, uploadfmt_t fmt, float hotx, float hoty, float scale)
{
    SDL_Surface *source, *resized;
    SDL_Cursor *cursor;
    SDL_PixelFormat format;
    int width = max(1, w*scale), height = max(1, h*scale);
    if (fmt == PTI_RGBA8 || fmt == PTI_RGBX8) format = SDL_PIXELFORMAT_RGBA32;
    else if (fmt == PTI_BGRA8 || fmt == PTI_BGRX8) format = SDL_PIXELFORMAT_BGRA32;
    else return NULL;
    source = SDL_CreateSurfaceFrom(w, h, format, (void *)pixels, w*4);
    if (!source) return NULL;
    resized = SDL_ScaleSurface(source, width, height, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(source);
    if (!resized) return NULL;
    cursor = SDL_CreateColorCursor(resized, bound(0, hotx*scale, width-1), bound(0, hoty*scale, height-1));
    SDL_DestroySurface(resized);
    return cursor;
}
static qboolean GPU_SetCursor(void *cursor) { return SDL_SetCursor(cursor); }
static void GPU_DestroyCursor(void *cursor) { SDL_DestroyCursor(cursor); }

static void GPU_DrawInit(void) { R2D_Init(); }
static void GPU_DrawShutdown(void) { R2D_Shutdown(); }
static void GPU_RenderInit(void) {}
static void GPU_RenderShutdown(void)
{
    R_GAliasFlushSkinCache(true);
    Surf_DeInit();
    Shader_Shutdown();
    Image_Shutdown();
}

void GPU_Set2D(void)
{
    Matrix4x4_CM_Orthographic(r_refdef.m_projection_std, 0, vid.width, vid.height, 0, 0, 99999);
    Matrix4x4_Identity(r_refdef.m_view);
    vid.fbvwidth = vid.width; vid.fbvheight = vid.height;
    vid.fbpwidth = gpu.targetwidth; vid.fbpheight = gpu.targetheight;
    r_refdef.pxrect.x = r_refdef.pxrect.y = 0;
    r_refdef.pxrect.width = vid.fbpwidth; r_refdef.pxrect.height = vid.fbpheight;
    gpu.drawing2d = true;
    GPU_SelectEntity(&r_worldentity);
    GPU_SelectMode(BEM_STANDARD);
    GPU_Scissor(NULL);
}

static void GPU_RenderView(void)
{
    extern cvar_t r_projection;
    gpu_panini_view_t *panini = NULL;
    qboolean project = r_projection.ival == PROJ_PANINI && !r_refdef.recurse &&
        r_refdef.useperspective && !(r_refdef.flags & RDF_NOWORLDMODEL);
    float fx = r_refdef.fov_x, fy = r_refdef.fov_y;
    if (r_norefresh.value) return;
    if (R2D_Flush) R2D_Flush();
    gpu.drawing2d = false;
    if (project) {
        fx = bound(0.001f, fx, 170.0f); fy = bound(0.001f, fy, 170.0f);
    }
    AngleVectors(r_refdef.viewangles, vpn, vright, vup);
    VectorCopy(r_refdef.vieworg, r_origin);
    if (r_viewcontents & FTECONTENTS_FLUID && r_waterwarp.value) {
        fx *= 1 + (sin(cl.time*4.7)+1)*0.015*r_waterwarp.value;
        fy *= 1 + (sin(cl.time*3.0)+1)*0.015*r_waterwarp.value;
    }
    Matrix4x4_CM_ModelViewMatrixFromAxis(r_refdef.m_view, vpn, vright, vup, r_refdef.vieworg);
    Matrix4x4_CM_Projection_Offset(r_refdef.m_projection_std, -fx/2, fx/2, -fy/2, fy/2,
        r_refdef.mindist, r_refdef.maxdist, true);
    Matrix4x4_CM_Projection_Offset(r_refdef.m_projection_view, -r_refdef.fovv_x/2, r_refdef.fovv_x/2,
        -r_refdef.fovv_y/2, r_refdef.fovv_y/2, r_refdef.mindist, r_refdef.maxdist, true);
    r_refdef.pxrect.x = floor(r_refdef.vrect.x*vid.fbpwidth/vid.width);
    r_refdef.pxrect.y = floor(r_refdef.vrect.y*vid.fbpheight/vid.height);
    r_refdef.pxrect.width = ceil(r_refdef.vrect.width*vid.fbpwidth/vid.width);
    r_refdef.pxrect.height = ceil(r_refdef.vrect.height*vid.fbpheight/vid.height);
    if (project && r_refdef.pxrect.width > 0 && r_refdef.pxrect.height > 0)
        panini = GPU_BeginPaniniView(fx);
    GPU_CheckLightingSettings();
    Surf_SetupFrame();
    if (!(r_refdef.flags & RDF_NOWORLDMODEL)) {
        if (r_viewcontents & FTECONTENTS_FLUID) r_refdef.flags |= RDF_UNDERWATER;
        else r_refdef.flags &= ~RDF_UNDERWATER;
    }
    if (!r_refdef.globalfog.density) {
        extern cvar_t r_fog_linear;
        int fogtype = ((r_refdef.flags & RDF_UNDERWATER) && cl.fog[FOGTYPE_WATER].density) ? FOGTYPE_WATER : FOGTYPE_AIR;
        CL_BlendFog(&r_refdef.globalfog, &cl.oldfog[fogtype], realtime, &cl.fog[fogtype]);
        if (!r_fog_linear.ival) r_refdef.globalfog.density /= 64;
    }
    R_SetFrustum(r_refdef.m_projection_std, r_refdef.m_view);
    RQ_BeginFrame();
    if ((r_refdef.flags & RDF_NOWORLDMODEL) ||
        (r_worldentity.model && r_worldentity.model->loadstate == MLS_LOADED && cl.worldmodel))
        Surf_DrawWorld();
    RQ_RenderBatchClear();
    if (panini) GPU_EndPaniniView(panini);
    GPU_Set2D();
}

static qboolean GPU_UpdateScreen(void)
{
    extern qboolean scr_initialized, scr_drawloading, scr_con_forcedraw;
    qboolean noworld = false, nohud = false;
    if (!scr_initialized || !con_initialized) return false;
    GPU_BeginFrame();
    if (!gpu.command || !gpu.swapchain) return false;
    R2D_Font_Changed();
    Shader_DoReload();
    GPU_Set2D();
    if (scr_disabled_for_loading) {
        extern float scr_disabled_time;
        if (Sys_DoubleTime() - scr_disabled_time <= 60 && !Key_Dest_Has(~kdm_game)) {
            scr_drawloading = true; SCR_DrawLoading(true); scr_drawloading = false;
            if (R2D_Flush) R2D_Flush();
            GPU_EndFrame(); return true;
        }
        scr_disabled_for_loading = false;
    }
    SCR_SetUpToDrawConsole();
    if (topmenu && topmenu->isopaque) nohud = true;
#ifdef VM_CG
    /* Cgame owns the camera, scene entities, prediction and HUD in Q3. */
    else if (q3 && q3->cg.Redraw(cl.time)) nohud = true;
#endif
#ifdef CSQC_DAT
    else if (CSQC_DrawView()) nohud = true;
#endif
    else if (r_worldentity.model && cls.state == ca_active) V_RenderView(nohud);
    else noworld = true;
    GPU_Set2D();
    scr_con_forcedraw = false;
    if (noworld) {
        if (!Key_Dest_Has(~(kdm_game|kdm_console)) && SCR_GetLoadingStage() == LS_NONE)
            scr_con_current = vid.height;
        if (scr_con_current != vid.height) R2D_ConsoleBackground(0, vid.height, true);
        else scr_con_forcedraw = true;
        nohud = true;
    }
    /* Plugin scene rendering releases its playerview before returning. */
    r_refdef.playerview = &cl.playerview[0];
    SCR_DrawTwoDimensional(nohud);
    V_UpdatePalette(false);
    RSpeedShow();
    if (R2D_Flush) R2D_Flush();
    GPU_EndFrame();
    Media_RecordFrame();
    return true;
}

static char *GPU_ReadPixels(int *stride, int *width, int *height, enum uploadfmt *fmt)
{
    SDL_GPUTransferBufferCreateInfo ti = {0};
    SDL_GPUTransferBuffer *transfer;
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *copy;
    SDL_GPUTextureRegion src = {0};
    SDL_GPUTextureTransferInfo dst = {0};
    SDL_GPUFence *fence;
    void *mapped;
    char *result;
    unsigned rowsize = (gpu.width*4 + 255) & ~255u, y;
    if (!gpu.present || !rowsize || !gpu.height) return NULL;
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD; ti.size = rowsize*gpu.height;
    transfer = SDL_CreateGPUTransferBuffer(gpu.device, &ti);
    if (!transfer) return NULL;
    cmd = SDL_AcquireGPUCommandBuffer(gpu.device);
    if (!cmd) { SDL_ReleaseGPUTransferBuffer(gpu.device, transfer); return NULL; }
    copy = SDL_BeginGPUCopyPass(cmd);
    src.texture = gpu.present; src.w = gpu.width; src.h = gpu.height; src.d = 1;
    dst.transfer_buffer = transfer; dst.pixels_per_row = rowsize/4; dst.rows_per_layer = gpu.height;
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);
    fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence || !SDL_WaitForGPUFences(gpu.device, true, &fence, 1)) {
        if (fence) SDL_ReleaseGPUFence(gpu.device, fence);
        SDL_ReleaseGPUTransferBuffer(gpu.device, transfer); return NULL;
    }
    mapped = SDL_MapGPUTransferBuffer(gpu.device, transfer, false);
    result = mapped ? BZ_Malloc(gpu.width*gpu.height*4) : NULL;
    if (mapped) {
        for (y = 0; y < gpu.height; ++y) memcpy(result+y*gpu.width*4, (char *)mapped+y*rowsize, gpu.width*4);
        SDL_UnmapGPUTransferBuffer(gpu.device, transfer);
    }
    SDL_ReleaseGPUFence(gpu.device, fence);
    SDL_ReleaseGPUTransferBuffer(gpu.device, transfer);
    *stride = gpu.width*4; *width = gpu.width; *height = gpu.height; *fmt = gpu.colorformat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || gpu.colorformat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB ? PTI_BGRA8 : PTI_RGBA8;
    return result;
}

rendererinfo_t sdlgpurendererinfo = {
    .description = "SDL3 GPU (Metal / Vulkan)", .name = {"sdlgpu", "SDL3GPU"}, .rtype = QR_SDLGPU,
    .Draw_Init = GPU_DrawInit, .Draw_Shutdown = GPU_DrawShutdown,
    .IMG_UpdateFiltering = GPU_UpdateFiltering, .IMG_LoadTextureMips = GPU_LoadTextureMips, .IMG_DestroyTexture = GPU_DestroyTexture,
    .R_Init = GPU_RenderInit, .R_DeInit = GPU_RenderShutdown, .R_RenderView = GPU_RenderView,
    .VID_Init = GPU_VideoInit, .VID_DeInit = GPU_VideoShutdown, .VID_SwapBuffers = GPU_EndFrame,
    .VID_ApplyGammaRamps = GPU_Gamma, .VID_CreateCursor = GPU_CreateCursor, .VID_SetCursor = GPU_SetCursor,
    .VID_DestroyCursor = GPU_DestroyCursor, .VID_SetWindowCaption = GPU_Caption, .VID_GetRGBInfo = GPU_ReadPixels,
    .SCR_UpdateScreen = GPU_UpdateScreen,
    .BE_SelectMode = GPU_SelectMode, .BE_DrawMesh_List = GPU_DrawMeshList, .BE_DrawMesh_Single = GPU_DrawMeshSingle,
    .BE_SubmitBatch = GPU_SubmitBatch, .BE_GetTempBatch = GPU_GetTempBatch, .BE_DrawWorld = GPU_DrawWorld,
    .BE_Init = GPU_BackendInit, .BE_GenBrushModelVBO = GPU_GenBrushModelVBO, .BE_ClearVBO = GPU_ClearVBO,
    .BE_UploadAllLightmaps = GPU_UploadAllLightmaps, .BE_SelectEntity = GPU_SelectEntity,
    .BE_SelectDLight = GPU_SelectDLight, .BE_Scissor = GPU_Scissor, .BE_LightCullModel = GPU_LightCullModel,
    .BE_VBO_Begin = GPU_VBOBegin, .BE_VBO_Data = GPU_VBOData, .BE_VBO_Finish = GPU_VBOFinish, .BE_VBO_Destroy = GPU_VBODestroy,
    .BE_RenderToTextureUpdate2d = GPU_RenderToTextureUpdate2D, .alignment = "no more", .VID_EnumerateVideoModes = GPU_VideoModes
};
