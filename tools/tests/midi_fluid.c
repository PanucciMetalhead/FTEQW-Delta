/* Tests the production parser and FluidSynth stream, without engine/game data.
 * Copyright (C) 2026 FTEQW Delta contributors. GPL-2.0-or-later. */
#include "midi_fluid.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%i: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

/* Format 0: tempo changes at quarter-note 1 from 120 to 60 BPM. Running status,
 * sustain, program changes and note-off via zero velocity are all exercised. */
static const unsigned char song[] = {
    'M','T','h','d',0,0,0,6, 0,0,0,1, 0,96,
    'M','T','r','k',0,0,0,39,
    0,0xff,0x51,3,7,0xa1,0x20,
    0,0xc0,0,
    0,0xb0,64,127,
    0,0x90,60,100,
    96,60,0,
    0,0xff,0x51,3,15,0x42,0x40,
    0,0x90,67,100,
    96,67,0,
    0,0xff,0x2f,0
};

static void parser(void)
{
    unsigned char copy[sizeof(song)];
    double seconds;
    CHECK(sizeof(song) == 61);
    CHECK(MIDI_Duration(song, sizeof(song), &seconds));
    CHECK(fabs(seconds - 1.5) < 0.00001);
    /* Every truncated prefix must fail, including track/event boundaries. */
    for (size_t i = 0; i < sizeof(song); ++i)
        CHECK(!MIDI_Duration(song, i, NULL));
    CHECK(!MIDI_Duration(NULL, 0, NULL));
    CHECK(!MIDI_Duration(song, MIDI_MAX_FILE + 1, NULL));
    memcpy(copy, song, sizeof(song));
    copy[9] = 2; CHECK(!MIDI_Duration(copy, sizeof(copy), NULL));
    copy[9] = 0; copy[12] = 128; CHECK(!MIDI_Duration(copy, sizeof(copy), NULL));
    copy[12] = 0; copy[13] = 0; CHECK(!MIDI_Duration(copy, sizeof(copy), NULL));
    memcpy(copy, song, sizeof(song));
    copy[21] = 255; CHECK(!MIDI_Duration(copy, sizeof(copy), NULL));
    memcpy(copy, song, sizeof(song));
    memset(copy + 22, 0xff, 5); CHECK(!MIDI_Duration(copy, sizeof(copy), NULL));
    memcpy(copy, song, sizeof(song));
    copy[23] = 60; CHECK(!MIDI_Duration(copy, sizeof(copy), NULL)); /* no running status */
    memcpy(copy, song, sizeof(song));
    copy[29] = 0x90; copy[31] = 0xff; CHECK(!MIDI_Duration(copy, sizeof(copy), NULL));
    memcpy(copy, song, sizeof(song));
    copy[59] = 0; CHECK(!MIDI_Duration(copy, sizeof(copy), NULL)); /* missing EOT */
    /* A format-1 tempo track ending before the note track. */
    {
        unsigned char multi[] = {
            'M','T','h','d',0,0,0,6,0,1,0,2,0,96,
            'M','T','r','k',0,0,0,11,
            0,0xff,0x51,3,15,0x42,0x40,0,0xff,0x2f,0,
            'M','T','r','k',0,0,0,12,
            0,0x90,60,100,96,60,0,0,0xff,0x2f,0,0
        };
        CHECK(MIDI_Duration(multi, sizeof(multi), &seconds));
        CHECK(fabs(seconds - 1) < 0.00001);
    }
    /* Deterministic malformed inputs through the actual production parser. */
    for (size_t i = 0; i < sizeof(copy); ++i)
        for (unsigned int j = 0; j < 256; ++j)
        {
            memcpy(copy, song, sizeof(copy));
            copy[i] = (unsigned char)j;
            if (MIDI_Duration(copy, sizeof(copy), &seconds))
                CHECK(seconds >= 0 && seconds <= 3600);
        }
    puts("midi: tempo map, format 0/1, running status, truncation and malformed inputs passed");
}

static void synth(const char *font, unsigned int rate)
{
    char error[256];
    midi_stream_t *s = MIDI_Open(song, sizeof(song), font, rate, error, sizeof(error));
    uint64_t total = (uint64_t)(rate * 3.5);
    unsigned int available;
    const short *pcm;
    long long energy = 0;
    short reference[4096 * 2];
    if (!s)
        fprintf(stderr, "%s\n", error);
    CHECK(s);
    CHECK(MIDI_Length(s) == total);
    for (uint64_t start = 0; start < total; start += 4096)
    {
        pcm = MIDI_Read(s, start, 4096, &available);
        CHECK(pcm && available && available <= MIDI_MAX_READ);
        if (start == 0)
            memcpy(reference, pcm, sizeof(reference));
        for (unsigned int i = 0; i < available * 2; ++i)
            energy += abs(pcm[i]);
    }
    CHECK(energy > 1000000);
    CHECK(!MIDI_Read(s, total, 1, &available) && available == 0);
    CHECK(!MIDI_Read(s, UINT64_MAX, 1, &available));
    CHECK(!MIDI_Read(s, 0, MIDI_MAX_READ + 1, &available));
    /* Rewind after EOF: held sustain and program state must not leak. Dither
     * changes between renders, so compare a small tolerance instead of hashes. */
    pcm = MIDI_Read(s, 0, 4096, &available);
    CHECK(pcm && available == 4096);
    {
        double difference = 0;
        for (unsigned int i = 0; i < 4096 * 2; ++i)
            difference += abs(pcm[i] - reference[i]);
        CHECK(difference / (4096 * 2) < 10);
    }
    /* Forward seek across the tempo change, then overlapping/backward reads. */
    CHECK(MIDI_Read(s, rate, 2048, &available));
    CHECK(MIDI_Read(s, rate - 1024, 2048, &available));
    CHECK(MIDI_Read(s, 128, 2048, &available));
    MIDI_Close(s);
    CHECK(!MIDI_Open(song, sizeof(song), "/nonexistent/fte-midi.sf2", rate, error, sizeof(error)));
    CHECK(!MIDI_Open(song, sizeof(song), font, 0, error, sizeof(error)));
    printf("midi: %u Hz FluidR3 synthesis, EOF, rewind, seek, overlap, missing font passed\n", rate);
}

int main(int argc, char **argv)
{
    parser();
    if (argc > 1)
    {
        synth(argv[1], 22050);
        synth(argv[1], 44100);
        synth(argv[1], 48000);
    }
    return 0;
}
