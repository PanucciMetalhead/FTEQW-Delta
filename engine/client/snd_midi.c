/* Copyright (C) 2026 FTEQW Delta contributors. GPL-2.0-or-later. */
#include "quakedef.h"
#ifdef AVAIL_MIDI
#include "midi_fluid.h"

#define MIDI_DEFAULT_FONT "soundfonts/FluidR3_GM.sf2"
static cvar_t snd_midi_soundfont = CVARFD("snd_midi_soundfont", MIDI_DEFAULT_FONT,
    CVAR_ARCHIVE | CVAR_NOTFROMSERVER,
    "SoundFont for MIDI music. A loose file relative to the executable, game directory, or home/base directory, or an absolute path. Changes apply to newly loaded tracks; use snd_restart to reload.");

void S_MIDI_Init(void)
{
    Cvar_Register(&snd_midi_soundfont, "Sound controls");
}

qboolean S_MIDI_SoundFontPath(char *path, size_t size)
{
    const char *name = snd_midi_soundfont.string;
    const enum fs_relative roots[] = {FS_BINARYPATH, FS_GAME, FS_ROOT};
    vfsfile_t *file;
    /* Only the explicit local cvar may select an absolute system file. */
    if (*name == '/' || *name == '\\' || (name[0] && name[1] == ':'))
        return FS_SystemPath(name, FS_SYSTEM, path, size);
    for (size_t i = 0; i < countof(roots); ++i)
    {
        if (!FS_SystemPath(name, roots[i], path, size))
            continue;
        file = FS_OpenVFS(path, "rb", FS_SYSTEM);
        if (file)
        {
            VFS_CLOSE(file);
            return true;
        }
    }
#ifdef FTE_MIDI_INSTALLED_FONT
    if (!strcmp(name, MIDI_DEFAULT_FONT))
    {
        file = FS_OpenVFS(FTE_MIDI_INSTALLED_FONT, "rb", FS_SYSTEM);
        if (file)
        {
            VFS_CLOSE(file);
            Q_strncpyz(path, FTE_MIDI_INSTALLED_FONT, size);
            return true;
        }
    }
#endif
    return false;
}

static void QDECL MIDI_Purge(sfx_t *s)
{
    MIDI_Close(s->decoder.buf);
    memset(&s->decoder, 0, sizeof(s->decoder));
    s->loadstate = SLS_NOTLOADED;
}

static float QDECL MIDI_Query(sfx_t *s, sfxcache_t *cache, char *title, size_t titlesize)
{
    midi_stream_t *stream = s->decoder.buf;
    if (cache)
    {
        memset(cache, 0, sizeof(*cache));
        cache->length = MIDI_Length(stream);
        cache->speed = MIDI_Rate(stream);
        cache->format = QAF_S16;
        cache->numchannels = 2;
    }
    if (title && titlesize)
        Q_strncpyz(title, COM_SkipPath(s->name), titlesize);
    return MIDI_Length(stream) / (float)MIDI_Rate(stream);
}

static sfxcache_t *QDECL MIDI_Decode(sfx_t *s, sfxcache_t *cache, ssamplepos_t start, int count)
{
    unsigned int available;
    const short *pcm;
    if (start < 0 || count < 0)
        return NULL;
    if (!count)
    {
        MIDI_Query(s, cache, NULL, 0);
        return cache;
    }
    pcm = MIDI_Read(s->decoder.buf, start, count, &available);
    if (!pcm || !available)
        return NULL;
    MIDI_Query(s, cache, NULL, 0);
    cache->data = (qbyte *)pcm;
    cache->soundoffset = start;
    cache->length = available;
    return cache;
}

qboolean QDECL S_LoadMIDISound(sfx_t *s, qbyte *data, size_t size, int speed, qboolean forcedecode)
{
    char path[MAX_OSPATH], error[192];
    midi_stream_t *stream;
    (void)forcedecode;
    if (size < 4 || memcmp(data, "MThd", 4))
        return false;
    if (!S_MIDI_SoundFontPath(path, sizeof(path)))
    {
        Con_Printf("MIDI: SoundFont not found; set snd_midi_soundfont or install " MIDI_DEFAULT_FONT "\n");
        return false;
    }
    stream = MIDI_Open(data, size, path, speed, error, sizeof(error));
    if (!stream)
    {
        Con_Printf("MIDI: %s: %s\n", s->name, error);
        return false;
    }
    s->loopstart = -1; /* The existing music playlist owns looping. */
    s->decoder.buf = stream;
    s->decoder.decodedata = MIDI_Decode;
    s->decoder.querydata = MIDI_Query;
    s->decoder.ended = MIDI_Purge;
    s->decoder.purge = MIDI_Purge;
    Con_DPrintf("MIDI: loaded %s (%.2f seconds, %i Hz)\n", s->name,
        MIDI_Length(stream) / (double)speed, speed);
    return true;
}
#endif
