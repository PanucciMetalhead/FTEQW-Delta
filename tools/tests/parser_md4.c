/* Exercise the actual MD4 callback table, not a reconstructed registration.
   Reference digests were independently checked with OpenSSL's legacy provider. */
#include <alloca.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char qbyte;
/* ENGINE_HASH_TYPES */
/* ENGINE_MD4_SOURCE */
/* ENGINE_FUNCTIONS */

static void check(const unsigned char *data, size_t length, const char *hex)
{
    unsigned char expected[16], actual[16];
    unsigned int folded = 0;
    for (unsigned int i = 0; i < 16; ++i) {
        unsigned int byte;
        assert(sscanf(hex+2*i, "%2x", &byte) == 1);
        expected[i] = byte;
        folded ^= byte << (8*(i%4));
    }
    assert(hash_md4.digestsize == 16);
    assert(CalcHash(&hash_md4, actual, sizeof(actual), data, length) == 16);
    assert(!memcmp(actual, expected, sizeof(actual)));
    assert(CalcHashInt(&hash_md4, data, length) == folded);
    void *context = malloc(hash_md4.contextsize);
    assert(context);
    hash_md4.init(context);
    for (size_t offset = 0; offset < length;) {
        size_t count = length - offset;
        if (count > 17) count = 17;
        hash_md4.process(context, data+offset, count);
        offset += count;
    }
    hash_md4.terminate(actual, context);
    assert(!memcmp(actual, expected, sizeof(actual)));
    free(context);
    memset(actual, 0xa5, sizeof(actual));
    assert(CalcHash(&hash_md4, actual, 15, data, length) == 0);
    for (unsigned int i = 0; i < sizeof(actual); ++i) assert(actual[i] == 0xa5);
}

int main(void)
{
    static const struct {const char *text, *digest;} vectors[] = {
        {"", "31d6cfe0d16ae931b73c59d7e0c089c0"},
        {"a", "bde52cb31de33e46245e05fbdbd6fb24"},
        {"abc", "a448017aaf21d8525fc10ae87aa6729d"},
        {"message digest", "d9130a8164549fe818874806e1c7014b"},
        {"abcdefghijklmnopqrstuvwxyz", "d79e1c308aa5bbcdeea8ed63df412da9"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "043f8582f241db351ce627e153e7f0e4"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890", "e33b4ddc9c38f2199c3e7b164fcc0536"},
    };
    for (size_t i = 0; i < sizeof(vectors)/sizeof(vectors[0]); ++i)
        check((const unsigned char *)vectors[i].text, strlen(vectors[i].text), vectors[i].digest);
    unsigned char *data = malloc(1000000);
    assert(data);
    memset(data, 'a', 1000000);
    static const struct {size_t length; const char *digest;} repeated[] = {
        {55, "c889c81dd86c4d2e025778944ea02881"},
        {56, "d5f9a9e9257077a5f08b0b92f348b0ad"},
        {63, "7ea3da77432d44c323671097d1348fc8"},
        {64, "52f5076fabd22680234a3fa9f9dc5732"},
        {65, "330e377bf231f3cacfecc2c182fe7e5b"},
        {127, "9733b046ad770b4e093b35de4e09e828"},
        {128, "cb4a20a561558e29460190c91dced59f"},
        {129, "2adcd303c29f93a3ee33a560ece91cd2"},
        {8193, "b904d9710fa802d2434a43f016f31093"},
        {1000000, "bbce80cc6bb65e5c6745e30d4eeca9a4"},
    };
    for (size_t i = 0; i < sizeof(repeated)/sizeof(repeated[0]); ++i)
        check(data, repeated[i].length, repeated[i].digest);
    free(data);
    puts("MD4: actual callback table, streaming, block/padding boundaries, million-byte input and purity checksum folding match reference digests");
    return 0;
}
