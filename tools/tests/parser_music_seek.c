/* Exercise the real seek body with guarded mixer state, without SDL/game data. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define MUSIC_FIRST 2
#define MUSIC_STOP 4
typedef struct { intptr_t pos; int rate; } channel_t;
typedef struct soundcardinfo_s {
    struct soundcardinfo_s *next;
    struct { int speed; } sn;
    channel_t channel[MUSIC_STOP];
} soundcardinfo_t;
static soundcardinfo_t *cards;
static int locked, acquisitions;
static void S_LockMixer(void)
{
    assert(!locked); locked = 1; ++acquisitions;
}
static void S_UnlockMixer(void)
{
    assert(locked); locked = 0;
}
static soundcardinfo_t *checked_cards(void)
{
    /* Checking the shared card list requires owning the mixer lock before
     * the first channel access, rather than acquiring it after a seek. */
    assert(locked);
    return cards;
}
#define sndcardinfo checked_cards()
/* ENGINE_FUNCTIONS */

int main(void)
{
    soundcardinfo_t b = {0}, a = {0};
    a.next = &b; a.sn.speed = 48000; b.sn.speed = 22050; cards = &a;
    a.channel[0].pos = b.channel[0].pos = 777;
    a.channel[2].rate = b.channel[2].rate = 64;
    a.channel[3].rate = b.channel[3].rate = 128;
    S_Music_Seek(750);
    assert(!locked && acquisitions == 1);
    assert(a.channel[2].pos == INT64_C(2304000000));
    assert(a.channel[3].pos == INT64_C(4608000000));
    assert(b.channel[2].pos == INT64_C(1058400000));
    assert(b.channel[3].pos == INT64_C(2116800000));
    assert(a.channel[0].pos == 777 && b.channel[0].pos == 777);
    S_Music_Seek(-1000);
    assert(!locked && acquisitions == 2);
    assert(!a.channel[2].pos && !a.channel[3].pos && !b.channel[2].pos && !b.channel[3].pos);
    cards = NULL; S_Music_Seek(15);
    assert(!locked && acquisitions == 3);
    puts("Music seek: locked channel access, 64-bit long seeks, rates, rewind clamp and empty cards passed");
    return 0;
}
