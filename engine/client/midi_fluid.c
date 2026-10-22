/* FluidSynth renders into the engine's mixer; it never opens an audio or MIDI
 * device or runs a wall-clock player thread.
 * Copyright (C) 2026 FTEQW Delta contributors. GPL-2.0-or-later. */
#include "midi_fluid.h"
#include <fluidsynth.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t tick, tempo, order; } midi_tempo_t;

static uint32_t MIDI_BE16(const unsigned char *p)
{
    return ((uint32_t)p[0] << 8) | p[1];
}
static uint32_t MIDI_BE32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static int MIDI_VLQ(const unsigned char **p, const unsigned char *end, uint32_t *value)
{
    *value = 0;
    for (int i = 0; i < 4 && *p < end; ++i)
    {
        unsigned int b = *(*p)++;
        *value = (*value << 7) | (b & 127);
        if (!(b & 128))
            return 1;
    }
    return 0;
}
static int MIDI_TempoOrder(const void *a, const void *b)
{
    const midi_tempo_t *x = a, *y = b;
    if (x->tick != y->tick)
        return x->tick < y->tick ? -1 : 1;
    return (x->order > y->order) - (x->order < y->order);
}

int MIDI_Duration(const void *data, size_t size, double *seconds)
{
    const unsigned char *p = data, *end;
    midi_tempo_t *tempos = NULL;
    uint32_t tracks, division, last = 0, events = 0, ntempos = 0, capacity = 0;
    double duration = 0;
    int valid = 0;
    if (!data || size < 14 || size > MIDI_MAX_FILE || memcmp(p, "MThd", 4) ||
        MIDI_BE32(p + 4) != 6 || MIDI_BE16(p + 8) > 1)
        return 0;
    tracks = MIDI_BE16(p + 10);
    division = MIDI_BE16(p + 12);
    if (!tracks || tracks > 128 || !division || (division & 0x8000) ||
        (!MIDI_BE16(p + 8) && tracks != 1))
        return 0;
    end = p + size;
    p += 14;
    for (uint32_t track = 0; track < tracks; ++track)
    {
        const unsigned char *trackend;
        uint32_t tick = 0, bytes;
        unsigned int running = 0;
        int ended = 0;
        if ((size_t)(end - p) < 8 || memcmp(p, "MTrk", 4))
            goto done;
        bytes = MIDI_BE32(p + 4);
        p += 8;
        if (bytes > (size_t)(end - p))
            goto done;
        trackend = p + bytes;
        while (p < trackend && !ended)
        {
            uint32_t delta, len;
            unsigned int status;
            if (++events > 1000000 || !MIDI_VLQ(&p, trackend, &delta) ||
                delta > (uint32_t)INT_MAX - tick || p == trackend)
                goto done;
            tick += delta;
            status = *p;
            if (status & 128)
                ++p;
            else if (running)
                status = running;
            else
                goto done;
            if (status < 0xf0)
            {
                running = status;
                len = ((status & 0xe0) == 0xc0) ? 1 : 2;
                if (len > (size_t)(trackend - p))
                    goto done;
                for (uint32_t i = 0; i < len; ++i)
                    if (p[i] & 128)
                        goto done;
                p += len;
            }
            else
            {
                unsigned int type = 0;
                running = 0;
                if (status == 0xff)
                {
                    if (p == trackend)
                        goto done;
                    type = *p++;
                }
                else if (status != 0xf0 && status != 0xf7)
                    goto done;
                if (!MIDI_VLQ(&p, trackend, &len) || len > (size_t)(trackend - p))
                    goto done;
                if (status == 0xff && type == 0x51)
                {
                    uint32_t tempo;
                    if (len != 3 || !(tempo = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]))
                        goto done;
                    if (ntempos == 65536)
                        goto done;
                    if (ntempos == capacity)
                    {
                        midi_tempo_t *grown;
                        capacity = capacity ? capacity * 2 : 32;
                        grown = realloc(tempos, capacity * sizeof(*tempos));
                        if (!grown)
                            goto done;
                        tempos = grown;
                    }
                    tempos[ntempos++] = (midi_tempo_t){tick, tempo, events};
                }
                if (status == 0xff && type == 0x2f)
                {
                    if (len)
                        goto done;
                    ended = 1;
                }
                p += len;
            }
        }
        if (!ended)
            goto done;
        if (tick > last)
            last = tick;
        p = trackend;
    }
    if (p != end)
        goto done;
    if (ntempos > 1)
        qsort(tempos, ntempos, sizeof(*tempos), MIDI_TempoOrder);
    {
        uint32_t tick = 0, tempo = 500000;
        for (uint32_t i = 0; i < ntempos; ++i)
        {
            duration += (tempos[i].tick - tick) * (double)tempo / (division * 1000000.0);
            tick = tempos[i].tick;
            tempo = tempos[i].tempo;
        }
        duration += (last - tick) * (double)tempo / (division * 1000000.0);
    }
    if (duration > 3600)
        goto done;
    if (seconds)
        *seconds = duration;
    valid = 1;
done:
    free(tempos);
    return valid;
}

struct midi_stream_s
{
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    fluid_player_t *player;
    unsigned char *file;
    size_t filesize;
    unsigned int rate, capacity, cached;
    uint64_t offset, position, length, rendered;
    short *pcm;
    int failed;
};

void MIDI_Close(midi_stream_t *s)
{
    if (!s)
        return;
    if (s->player)
        delete_fluid_player(s->player);
    if (s->synth)
        delete_fluid_synth(s->synth);
    if (s->settings)
        delete_fluid_settings(s->settings);
    free(s->file);
    free(s->pcm);
    free(s);
}

static int MIDI_Rewind(midi_stream_t *s)
{
    if (s->player)
    {
        delete_fluid_player(s->player);
        s->player = NULL;
        fluid_synth_system_reset(s->synth);
        /* FluidSynth retains a partial synthesis block. Consume it before a
         * new player's clock starts, so rewind doesn't expose old PCM or shift
         * the opening notes by the unused part of that block. */
        {
            short discard[128 * 2];
            int block = fluid_synth_get_internal_bufsize(s->synth);
            int drain = block - (int)(s->rendered % block);
            if (drain < block)
            {
                if (drain > 128 || fluid_synth_write_s16(s->synth, drain,
                        discard, 0, 2, discard, 1, 2) != FLUID_OK)
                    return 0;
                s->rendered += drain;
            }
        }
    }
    s->player = new_fluid_player(s->synth);
    s->offset = s->position = s->cached = 0;
    return s->player && fluid_player_add_mem(s->player, s->file, s->filesize) == FLUID_OK &&
        fluid_player_play(s->player) == FLUID_OK;
}

midi_stream_t *MIDI_Open(const void *data, size_t size, const char *soundfont,
    unsigned int rate, char *error, size_t errorsize)
{
    double duration;
    midi_stream_t *s = NULL;
    const char *message = "Invalid MIDI (requires format 0/1, PPQN, <=16 MiB, <=1 hour)";
    if (!MIDI_Duration(data, size, &duration))
        goto fail;
    message = "Unsupported MIDI sample rate (8000..192000 Hz required)";
    if (rate < 8000 || rate > 192000)
        goto fail;
    message = "Unable to allocate MIDI synthesizer";
    s = calloc(1, sizeof(*s));
    if (!s)
        goto fail;
    s->rate = rate;
    s->length = (uint64_t)ceil(duration * rate) + 2 * rate;
    s->filesize = size;
    s->file = malloc(size);
    s->settings = new_fluid_settings();
    if (!s->file || !s->settings)
        goto fail;
    memcpy(s->file, data, size);
    /* One owner thread per stream; playback advances only with rendered PCM. */
    if (fluid_settings_setnum(s->settings, "synth.sample-rate", rate) != FLUID_OK ||
        fluid_settings_setstr(s->settings, "player.timing-source", "sample") != FLUID_OK ||
        fluid_settings_setint(s->settings, "synth.threadsafe-api", 0) != FLUID_OK ||
        fluid_settings_setint(s->settings, "synth.cpu-cores", 1) != FLUID_OK)
        goto fail;
    s->synth = new_fluid_synth(s->settings);
    if (!s->synth)
        goto fail;
    message = "Unable to load MIDI SoundFont";
    if (!soundfont || !*soundfont || fluid_synth_sfload(s->synth, soundfont, 1) < 0)
        goto fail;
    message = "FluidSynth could not start MIDI playback";
    if (!MIDI_Rewind(s))
        goto fail;
    {
        unsigned int available;
        /* Parsing in FluidSynth happens on its first sample-clock callback.
         * Prime a block on the loader thread and catch decoder-level failures. */
        if (!MIDI_Read(s, 0, 64, &available) || fluid_player_get_status(s->player) == FLUID_PLAYER_DONE)
            goto fail;
    }
    return s;
fail:
    if (error && errorsize)
        snprintf(error, errorsize, "%s", message);
    MIDI_Close(s);
    return NULL;
}

uint64_t MIDI_Length(const midi_stream_t *s)
{
    return s->length;
}

unsigned int MIDI_Rate(const midi_stream_t *s)
{
    return s->rate;
}

const short *MIDI_Read(midi_stream_t *s, uint64_t start, unsigned int count,
    unsigned int *available)
{
    short discard[4096 * 2];
    uint64_t end;
    *available = 0;
    if (s->failed || start >= s->length || count > MIDI_MAX_READ)
        return NULL;
    if (count > s->length - start)
        count = (unsigned int)(s->length - start);
    end = start + count;
    if (start < s->offset && !MIDI_Rewind(s))
        goto fail;
    /* Keep a second of history for sound devices whose clocks differ slightly. */
    if (start > s->offset + s->rate)
    {
        uint64_t trim = start - s->offset - s->rate;
        if (trim > s->cached)
            trim = s->cached;
        if (trim && trim < s->cached)
            memmove(s->pcm, s->pcm + trim * 2, (s->cached - (size_t)trim) * 2 * sizeof(short));
        s->offset += trim;
        s->cached -= (unsigned int)trim;
    }
    /* Seeking forward renders discarded blocks to preserve envelopes/controllers. */
    if (start > s->position)
    {
        while (s->position < start)
        {
            unsigned int frames = (unsigned int)((start - s->position) > 4096 ? 4096 : start - s->position);
            if (fluid_synth_write_s16(s->synth, (int)frames, discard, 0, 2, discard, 1, 2) != FLUID_OK)
                goto fail;
            s->position += frames;
            s->rendered += frames;
        }
        s->offset = start;
        s->cached = 0;
    }
    if (end > s->position)
    {
        unsigned int needed = (unsigned int)(end - s->offset);
        if (needed > s->capacity)
        {
            short *grown = realloc(s->pcm, (size_t)needed * 2 * sizeof(short));
            if (!grown)
                goto fail;
            s->pcm = grown;
            s->capacity = needed;
        }
        if (fluid_synth_write_s16(s->synth, (int)(end - s->position),
                s->pcm + s->cached * 2, 0, 2, s->pcm + s->cached * 2, 1, 2) != FLUID_OK)
            goto fail;
        s->rendered += end - s->position;
        s->position = end;
        s->cached = needed;
    }
    *available = (unsigned int)(s->position - start);
    return s->pcm ? s->pcm + (size_t)(start - s->offset) * 2 : NULL;
fail:
    s->failed = 1;
    return NULL;
}
