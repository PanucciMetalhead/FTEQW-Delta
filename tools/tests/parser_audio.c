/* Exercise the actual signed PCM resampler, including negative full-scale samples. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef enum { QAF_S8 = 1, QAF_S16 = 2 } qaudiofmt_t;
/* ENGINE_AUDIO_MACROS */
/* ENGINE_FUNCTIONS */

static int sample(const void *data, qaudiofmt_t format, int index)
{
    return format == QAF_S8 ? ((const signed char *)data)[index] : ((const short *)data)[index];
}

static void store(void *data, qaudiofmt_t format, int index, int value)
{
    if (format == QAF_S8)
        ((signed char *)data)[index] = value;
    else
        ((short *)data)[index] = value;
}

static void conversions(void)
{
    signed char small[256], roundtrip[256];
    short wide[256];
    for (int i = 0; i < 256; ++i)
        small[i] = (signed char)(i - 128);
    SND_ResampleStream(small, 44100, QAF_S8, 1, 256, wide, 44100, QAF_S16, 1, 0);
    for (int i = 0; i < 256; ++i)
        assert(wide[i] == (i - 128) * 256);
    SND_ResampleStream(wide, 44100, QAF_S16, 1, 256, roundtrip, 44100, QAF_S8, 1, 0);
    assert(!memcmp(small, roundtrip, sizeof(small)));
    short *full = malloc(65536 * sizeof(*full));
    signed char *reduced = malloc(65536);
    assert(full && reduced);
    for (int i = 0; i < 65536; ++i)
        full[i] = (short)(i - 32768);
    SND_ResampleStream(full, 48000, QAF_S16, 2, 32768, reduced, 48000, QAF_S8, 2, 0);
    for (int i = 0; i < 65536; ++i)
        assert(reduced[i] == i / 256 - 128);
    free(full);
    free(reduced);
}

static void resampling(void)
{
    const int rates[] = {11025, 22050, 44100, 48000, 96000};
    const int frames = 1024;
    int cases = 0;
    for (int inf = 1; inf <= 2; ++inf)
    for (int outf = 1; outf <= 2; ++outf)
    for (int ir = 0; ir < 5; ++ir)
    for (int orate = 0; orate < 5; ++orate)
    for (int style = 0; style < 3; ++style)
    {
        qaudiofmt_t informat = (qaudiofmt_t)inf, outformat = (qaudiofmt_t)outf;
        int inrate = rates[ir], outrate = rates[orate];
        int capacity = (int)ceil(frames * (double)outrate / inrate) + 2;
        void *left = malloc(frames * inf), *right = malloc(frames * inf);
        void *stereo = malloc(frames * 2 * inf);
        void *lout = malloc(capacity * outf), *rout = malloc(capacity * outf);
        void *sout = malloc(capacity * 2 * outf);
        assert(left && right && stereo && lout && rout && sout);
        /* Alternating channel polarity and full-scale endpoints expose signed
           shifts while making cross-channel mixing observable. */
        const int values8[] = {-128, -1, 0, 1, 127};
        const int values16[] = {-32768, -204, -1, 0, 1, 203, 32767};
        for (int i = 0; i < frames; ++i)
        {
            int l = inf == 1 ? values8[i % 5] : values16[i % 7];
            int r = inf == 1 ? values8[(i + 2) % 5] : values16[(i + 3) % 7];
            store(left, informat, i, l);
            store(right, informat, i, r);
            store(stereo, informat, 2 * i, l);
            store(stereo, informat, 2 * i + 1, r);
        }
        memset(lout, 0xa5, capacity * outf);
        memset(rout, 0xa5, capacity * outf);
        memset(sout, 0xa5, capacity * 2 * outf);
        SND_ResampleStream(left, inrate, informat, 1, frames, lout, outrate, outformat, 1, style);
        SND_ResampleStream(right, inrate, informat, 1, frames, rout, outrate, outformat, 1, style);
        SND_ResampleStream(stereo, inrate, informat, 2, frames, sout, outrate, outformat, 2, style);
        for (int i = 0; i < capacity; ++i)
        {
            assert(sample(sout, outformat, 2 * i) == sample(lout, outformat, i));
            assert(sample(sout, outformat, 2 * i + 1) == sample(rout, outformat, i));
        }
        /* Nearest-neighbor doubling has an exact independent sample oracle. */
        if (!style && outrate == 2 * inrate)
            for (int i = 0; i < frames * 2; ++i)
            {
                int expected = sample(left, informat, i / 2);
                if (inf == 1 && outf == 2) expected *= 256;
                if (inf == 2 && outf == 1) expected = (int)floor(expected / 256.0);
                assert(sample(lout, outformat, i) == expected);
            }
        for (int i = (capacity - 1) * outf; i < capacity * outf; ++i)
            assert(((unsigned char *)lout)[i] == 0xa5);
        free(left); free(right); free(stereo);
        free(lout); free(rout); free(sout);
        ++cases;
    }
    printf("audio: exhaustive signed PCM conversion and %d mono/stereo resampling cases passed\n", cases);
}

int main(void)
{
    conversions();
    resampling();
    return 0;
}
