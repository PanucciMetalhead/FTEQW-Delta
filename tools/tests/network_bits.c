/* Exercise the actual engine signed-width normalization under UBSan.
 * Writer stubs capture the normalized field before protocol-specific packing. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define HUFFNETWORK
enum { SZ_BAD, SZ_RAWBYTES, SZ_RAWBITS, SZ_HUFFMAN };
enum { false, true };
typedef struct {
    int packing, maxsize, cursize, allowoverflow, overflowed;
} sizebuf_t;
static int written_value, written_bits, writes;
static void capture(sizebuf_t *msg, int value, int bits)
{
    (void)msg;
    written_value = value;
    written_bits = bits;
    writes++;
}
#define MSG_WriteRawBytes capture
#define MSG_WriteRawBits capture
#define MSG_WriteHuffBits capture
static void Sys_Error(const char *format, int bits)
{
    fprintf(stderr, format, bits);
    abort();
}

/* ENGINE_FUNCTIONS */

int main(void)
{
    for (int mode = SZ_RAWBYTES; mode <= SZ_HUFFMAN; mode++) {
        sizebuf_t msg = { .packing = mode, .maxsize = 1024 };
        for (int width = 1; width <= 31; width++) {
            int64_t sign = INT64_C(1) << (width - 1);
            int values[] = { (int)-sign, -1, 0, (int)(sign-1),
                             (int)sign, (int)((sign*2)-1) };
            uint32_t mask = (UINT32_C(1) << width) - 1;
            for (unsigned int i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
                uint32_t raw = (uint32_t)values[i] & mask;
                int expected = (int)((int64_t)raw -
                    ((int64_t)raw >= sign ? sign*2 : 0));
                writes = 0;
                MSG_WriteBits(&msg, values[i], -width);
                assert(writes == 1 && written_bits == width);
                assert(written_value == expected);
                /* Positive-width fields retain their original input. */
                writes = 0;
                MSG_WriteBits(&msg, values[i], width);
                assert(writes == 1 && written_bits == width);
                assert(written_value == values[i]);
            }
        }
        writes = 0;
        MSG_WriteBits(&msg, INT32_MIN, 32);
        assert(writes == 1 && written_bits == 32 && written_value == INT32_MIN);
    }
    puts("Signed packet widths 1-31 and positive fields passed in all packing modes");
}
