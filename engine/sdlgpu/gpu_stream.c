/* Budgeted whole-mip streaming for SDL GPU. GPL-2.0-or-later. */
#include "gpu_local.h"
#include <limits.h>

/* One temporary file, not one open handle per texture. Loader threads prepare
 * mip data once, including embedded BSP textures. No full-resolution CPU copy
 * survives upload. The file is deleted on close and never replaces game assets.
 * Cache space is bounded; sources beyond the cap use ordinary resident loading. */
#define GPU_STREAM_CACHE_LIMIT ((qofs_t)1024*1024*1024)
#define GPU_STREAM_MAX_JOBS 2
typedef struct gpu_stream_s {
    struct gpu_stream_s *next;
    image_t *owner;
    struct pendingtextureinfo info;
    qofs_t offset[72];
    size_t bytes[72];
    unsigned floor, resident, target, wanted, seen, demandframe, readmip;
    double lastuse, changed, retry;
    int pending;
    struct pendingtextureinfo *ready;
} gpu_stream_t;

static cvar_t gpu_texturestream = CVARFD("gpu_texturestream", "0", CVAR_ARCHIVE,
    "Stream world texture mipmaps to a memory budget. Enable then vid_restart to prepare textures. Zero restores full detail.");
static cvar_t gpu_texturebudget = CVARFD("gpu_texturebudget", "128", CVAR_ARCHIVE,
    "Texture allocation budget in MiB, including pinned images. Render targets created by the renderer are separate. Minimum mipmaps and pinned images can exceed this budget.");
static cvar_t gpu_textureupload = CVARFD("gpu_textureupload", "8", CVAR_ARCHIVE,
    "Streamed texture upload budget in MiB per frame. One larger texture may upload to ensure progress; at most two reads are outstanding.");
static gpu_stream_t *sources;
static void *cachemutex;
static vfsfile_t *cache;
static qofs_t cacheend;
static unsigned cacherefs, frame, jobs, uploads, failures;
static size_t uploadbytes;
static qboolean preparing;
static double nextplan, frametime;

static void GPU_StreamFreeMips(struct pendingtextureinfo *mips)
{
    if (!mips) return;
    BZ_Free(mips->extrafree);
    Z_Free(mips);
}

qboolean GPU_StreamActive(void) { return preparing && gpu_texturestream.ival != 0; }

void GPU_StreamRegister(void)
{
    cvar_t *vars[] = {&gpu_texturestream, &gpu_texturebudget, &gpu_textureupload};
    unsigned i;
    for (i = 0; i < countof(vars); ++i)
        if (Cvar_FindVar(vars[i]->name) != vars[i]) Cvar_Register(vars[i], "SDL GPU");
}

void GPU_StreamInit(void)
{
    cachemutex = Sys_CreateMutex();
    preparing = gpu_texturestream.ival != 0;
    frame = jobs = uploads = failures = 0; uploadbytes = 0; nextplan = 0;
    frametime = Sys_DoubleTime();
}

/* Called before the initial upload is queued, normally on a loader thread.
 * Only this texture's loading job accesses tex->gpustream until TEX_LOADED. */
void GPU_StreamPrepare(texid_t tex, const struct pendingtextureinfo *mips)
{
    gpu_stream_t *s;
    unsigned i;
    size_t total = 0;
    qboolean okay = true;
    if (!preparing || tex->gpustream || !(tex->flags & IF_WORLDTEX) ||
        (tex->flags & (IF_UIPIC|IF_RENDERTARGET|IF_NOMIPMAP|IF_NOPURGE)) ||
        mips->type != PTI_2D || mips->mipcount < 2 || mips->mipcount > 72 ||
        max(mips->mip[0].width,mips->mip[0].height) < 256 ||
        GPU_TextureFormat(mips->encoding) == SDL_GPU_TEXTUREFORMAT_INVALID)
        return;
    for (i = 0; i < mips->mipcount; ++i) {
        if (!mips->mip[i].data || !mips->mip[i].datasize || mips->mip[i].depth != 1 ||
            mips->mip[i].datasize > INT_MAX || total > INT_MAX-mips->mip[i].datasize)
            return;
        total += mips->mip[i].datasize;
    }
    s = Z_Malloc(sizeof(*s)); s->info = *mips; s->info.extrafree = NULL;
    for (i = mips->mipcount; i-- > 0;) {
        s->info.mip[i].data = NULL; s->info.mip[i].needfree = false;
        s->bytes[i] = mips->mip[i].datasize + (i+1 < mips->mipcount ? s->bytes[i+1] : 0);
    }
    while (s->floor+1 < mips->mipcount &&
        max(mips->mip[s->floor].width,mips->mip[s->floor].height) > 64) ++s->floor;
    Sys_LockMutex(cachemutex);
    if (cacheend + total > GPU_STREAM_CACHE_LIMIT) okay = false;
    if (okay && !cache) cache = FS_OpenTemp();
    if (!cache) okay = false;
    if (okay) {
        okay = VFS_SEEK(cache,cacheend);
        for (i = 0; okay && i < mips->mipcount; ++i) {
            s->offset[i] = cacheend;
            okay = VFS_WRITE(cache,mips->mip[i].data,mips->mip[i].datasize) == mips->mip[i].datasize;
            cacheend += mips->mip[i].datasize;
        }
        if (okay) ++cacherefs;
    }
    Sys_UnlockMutex(cachemutex);
    if (okay) tex->gpustream = s;
    else Z_Free(s); /* Disk failure leaves the original loading path intact. */
}

void GPU_StreamDestroy(texid_t tex)
{
    gpu_stream_t *s = tex->gpustream, **link;
    if (!s) return;
    /* Purging/restarting may wait; ordinary frame rendering never waits on IO. */
    COM_WorkerPartialSync(s, &s->pending, 1);
    for (link = &sources; *link && *link != s; link = &(*link)->next) {}
    if (*link) *link = s->next;
    GPU_StreamFreeMips(s->ready);
    Sys_LockMutex(cachemutex);
    if (!--cacherefs) {
        if (cache) VFS_CLOSE(cache);
        cache = NULL; cacheend = 0;
    }
    Sys_UnlockMutex(cachemutex);
    Z_Free(s); tex->gpustream = NULL;
}

void GPU_StreamShutdown(void)
{
    /* Image_Shutdown normally removed all sources before device destruction. */
    while (sources) GPU_StreamDestroy(sources->owner);
    if (cache) VFS_CLOSE(cache);
    cache = NULL; cacheend = 0; cacherefs = 0; preparing = false;
    if (cachemutex) Sys_DestroyMutex(cachemutex);
    cachemutex = NULL;
}

qboolean GPU_LoadTextureMips(texid_t tex, const struct pendingtextureinfo *mips)
{
    gpu_stream_t *s = tex->gpustream;
    struct pendingtextureinfo view;
    if (s && s->owner) { /* A dynamic rewrite invalidates the cached contents. */
        GPU_StreamDestroy(tex); s = NULL;
    }
    if (!s) return GPU_UploadTextureMips(tex,mips);
    s->owner = tex; s->next = sources; sources = s;
    s->resident = s->target = s->wanted = GPU_StreamActive() ? s->floor : 0;
    s->changed = Sys_DoubleTime();
    view = *mips;
    view.mipcount -= s->resident;
    memmove(view.mip,view.mip+s->resident,view.mipcount*sizeof(view.mip[0]));
    if (GPU_UploadTextureMips(tex,&view)) {
        GPU_SetImageMip(tex,s->resident);
        return true;
    }
    GPU_DestroyTexture(tex);
    return false;
}

void GPU_StreamBind(texid_t tex)
{
    if (!TEXLOADED(tex)) return;
    gpu_stream_t *s = tex->gpustream;
    if (s && s->owner && GPU_StreamActive()) {
        s->seen = frame; s->lastuse = frametime;
    }
}

void GPU_StreamTouch(texid_t tex, float uvperpixel)
{
    if (!TEXLOADED(tex)) return;
    gpu_stream_t *s = tex->gpustream;
    unsigned wanted = 0;
    if (!s || !s->owner || !GPU_StreamActive()) return;
    if (isfinite(uvperpixel) && uvperpixel > 0) {
        /* Reserve one extra mip for anisotropy, grazing angles and estimation
         * error. Normalized UVs stay unchanged when the allocation is resized. */
        float footprint = uvperpixel * max(s->info.mip[0].width,s->info.mip[0].height);
        while (wanted < s->floor && footprint >= 4) { ++wanted; footprint *= 0.5f; }
    }
    s->wanted = s->demandframe == frame ? min(s->wanted,wanted) : wanted;
    s->demandframe = s->seen = frame; s->lastuse = frametime;
}

static void GPU_StreamReadDone(void *ctx, void *data, size_t mip, size_t unused)
{
    gpu_stream_t *s = ctx;
    s->ready = data; s->readmip = mip; s->pending = 0; --jobs;
    if (!data) { ++failures; s->retry = Sys_DoubleTime()+5; }
}

static void GPU_StreamRead(void *ctx, void *unused, size_t mip, size_t unused2)
{
    gpu_stream_t *s = ctx;
    struct pendingtextureinfo *mips = Z_Malloc(sizeof(*mips));
    qbyte *data;
    size_t offset = 0;
    unsigned i;
    qboolean okay;
    *mips = s->info; mips->mipcount -= mip;
    memmove(mips->mip,mips->mip+mip,mips->mipcount*sizeof(mips->mip[0]));
    data = mips->extrafree = BZ_Malloc(s->bytes[mip]);
    Sys_LockMutex(cachemutex);
    okay = cache && VFS_SEEK(cache,s->offset[mip]) &&
        VFS_READ(cache,data,s->bytes[mip]) == s->bytes[mip];
    Sys_UnlockMutex(cachemutex);
    for (i = 0; i < mips->mipcount; ++i) {
        mips->mip[i].data = data+offset; offset += mips->mip[i].datasize;
    }
    if (!okay) { GPU_StreamFreeMips(mips); mips = NULL; }
    COM_AddWork(WG_MAIN,GPU_StreamReadDone,s,mips,mip,0);
}

static size_t GPU_StreamBudget(void)
{
    double mib = gpu_texturebudget.value;
    if (!isfinite(mib)) mib = 128;
    return (size_t)(bound(1,mib,16384)*1024*1024);
}

static void GPU_StreamPlan(double now)
{
    gpu_stream_t *s;
    size_t resident = 0, planned = 0, budget = GPU_StreamBudget();
    for (s = sources; s; s = s->next) {
        resident += GPU_ImageBytes(s->owner);
        if (!GPU_StreamActive()) s->target = 0;
        else if (now-s->lastuse > 2) s->target = s->floor;
        else s->target = s->wanted;
        planned += s->bytes[s->target];
    }
    planned += GPU_TextureBytes()-min(resident,GPU_TextureBytes());
    /* Sacrifice unseen textures first, then detail beyond the requested mip.
     * This is a soft allocation budget: pinned images/minimum mips may exceed it. */
    while (GPU_StreamActive() && planned > budget) {
        gpu_stream_t *victim = NULL;
        double lowest = 1e30;
        for (s = sources; s; s = s->next) if (s->target < s->floor) {
            double importance = (now-s->lastuse < 2 ? 1000000 : 0) +
                (s->target-s->wanted)*1024.0 + 1.0/(1.0+max(0,now-s->lastuse));
            if (importance < lowest) { lowest = importance; victim = s; }
        }
        if (!victim) break;
        planned -= victim->bytes[victim->target]-victim->bytes[victim->target+1];
        ++victim->target;
    }
}

void GPU_StreamFrame(void)
{
    gpu_stream_t *s;
    double now = Sys_DoubleTime(), mib = gpu_textureupload.value;
    size_t used = 0, limit;
    unsigned applied = 0, pass;
    frametime = now;
    if (!sources) { ++frame; return; }
    if (!isfinite(mib)) mib = 8;
    limit = (size_t)(bound(1,mib,256)*1024*1024);
    /* A binding without an explicit footprint (generated UVs, sky, entities,
     * shadow-only use, etc.) conservatively requests full detail. */
    for (s = sources; s; s = s->next)
        if (s->seen == frame && s->demandframe != frame) s->wanted = 0;
    if (now >= nextplan) { GPU_StreamPlan(now); nextplan = now+0.2; }
    /* Apply before any draw captures native texture/sampler handles. Build the
     * replacement transactionally, so allocation/upload failure keeps the old
     * image. SDL retires the old GPU objects after their submitted work ends. */
    for (s = sources; s; s = s->next) if (s->ready) {
        struct pendingtextureinfo *ready = s->ready;
        size_t bytes = s->bytes[s->readmip];
        if (s->readmip != s->target) {
            GPU_StreamFreeMips(ready); s->ready = NULL; continue;
        }
        if (applied && bytes > limit-min(used,limit)) continue;
        image_t replacement = *s->owner;
        replacement.gpuimage = NULL; replacement.gpustream = NULL;
        if (GPU_UploadTextureMips(&replacement,ready)) {
            GPU_SetImageMip(&replacement,s->readmip);
            image_t old = *s->owner;
            old.gpustream = NULL;
            s->owner->gpuimage = replacement.gpuimage;
            GPU_DestroyTexture(&old);
            s->resident = s->readmip; s->changed = now;
            ++uploads; uploadbytes += bytes;
        } else {
            GPU_DestroyTexture(&replacement); ++failures; s->retry = now+5;
        }
        GPU_StreamFreeMips(ready); s->ready = NULL;
        used += bytes; ++applied;
    }
    /* Demote before promoting. Reads have their own bounded queue, including
     * completed jobs awaiting upload, to bound staging RAM as well as GPU IO. */
    unsigned outstanding = jobs;
    size_t staging = 0, readlimit = max(16*1024*1024,limit*2);
    for (s = sources; s; s = s->next) {
        if (s->ready) ++outstanding;
        if (s->pending || s->ready) staging += s->bytes[s->readmip];
    }
    for (pass = 0; pass < 2 && outstanding < GPU_STREAM_MAX_JOBS; ++pass)
        for (s = sources; s && outstanding < GPU_STREAM_MAX_JOBS; s = s->next) {
            if (s->pending || s->ready || s->resident == s->target || now < s->retry ||
                (s->target > s->resident && now-s->changed < 1) ||
                ((s->target > s->resident) != (pass == 0))) continue;
            if (outstanding && s->bytes[s->target] > readlimit-min(staging,readlimit)) continue;
            s->readmip = s->target;
            staging += s->bytes[s->target];
            s->pending = 1; ++jobs; ++outstanding;
            COM_AddWork(WG_LOADER,GPU_StreamRead,s,NULL,s->target,0);
        }
    ++frame;
}

void GPU_StreamInfo(void)
{
    gpu_stream_t *s;
    unsigned count = 0, reduced = 0, pending = 0;
    size_t resident = 0, full = 0, staging = 0;
    qofs_t diskbytes;
    for (s = sources; s; s = s->next) {
        ++count; reduced += s->resident != 0; pending += s->pending || s->ready;
        resident += GPU_ImageBytes(s->owner); full += s->bytes[0];
        if (s->ready) staging += s->bytes[s->readmip];
    }
    Sys_LockMutex(cachemutex); diskbytes = cacheend; Sys_UnlockMutex(cachemutex);
    Con_Printf("Texture streaming: %s; %.3f MiB allocated, %.3f MiB budget; %u managed, %u reduced; %.3f/%.3f MiB resident/full; %u pending, %.3f MiB ready; %u uploads, %.3f MiB uploaded, %u failures; %.3f MiB temporary cache\n",
        GPU_StreamActive() ? "enabled" : "disabled", GPU_TextureBytes()/1048576.0,
        GPU_StreamBudget()/1048576.0, count,reduced,resident/1048576.0,full/1048576.0,
        pending,staging/1048576.0,uploads,uploadbytes/1048576.0,failures,diskbytes/1048576.0);
}
