/* MIDI synthesis shared by the engine decoder and standalone regression tests.
 * Copyright (C) 2026 FTEQW Delta contributors. GPL-2.0-or-later. */
#ifndef FTE_MIDI_FLUID_H
#define FTE_MIDI_FLUID_H
#include <stddef.h>
#include <stdint.h>

#define MIDI_MAX_FILE (16u * 1024u * 1024u)
#define MIDI_MAX_READ 262144u
typedef struct midi_stream_s midi_stream_t;

/* Standard MIDI format 0/1, PPQN timing. Returns zero for invalid/unsupported
 * input. Duration excludes the fixed two-second release/reverb tail. */
int MIDI_Duration(const void *data, size_t size, double *seconds);
midi_stream_t *MIDI_Open(const void *data, size_t size, const char *soundfont,
    unsigned int rate, char *error, size_t errorsize);
void MIDI_Close(midi_stream_t *stream);
uint64_t MIDI_Length(const midi_stream_t *stream);
unsigned int MIDI_Rate(const midi_stream_t *stream);
/* Returned interleaved stereo S16 belongs to stream, valid until the next call.
 * Seeking uses sample-clock rendering, including tempo and controller changes.
 * Calls for one stream must be serialized (the engine's mixer lock does this). */
const short *MIDI_Read(midi_stream_t *stream, uint64_t start, unsigned int count,
    unsigned int *available);
#endif
