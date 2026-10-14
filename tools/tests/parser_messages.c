/* Real engine bit/byte packing and buffer allocation, including short input. */
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char qbyte;
typedef enum { false, true } qboolean;
enum { SZ_BAD, SZ_RAWBYTES, SZ_RAWBITS, SZ_HUFFMAN };
typedef struct {
    qbyte *data;
    int maxsize, cursize, currentbit, packing;
    qboolean allowoverflow, overflowed;
} sizebuf_t;
static sizebuf_t *msg_readmsg;
static qboolean msg_badread;
static jmp_buf failure;
static int expected_error;
static _Noreturn void Sys_Error(const char *format, ...)
{
    (void)format;
    assert(expected_error);
    longjmp(failure, 1);
}
#define Host_EndGame Sys_Error
static void Sys_Printf(const char *format, ...) { (void)format; }
/* ENGINE_FUNCTIONS */

static uint32_t state = 0x81234567u;
static uint32_t random_value(void)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}

static void packet(int mode, int width, uint32_t value)
{
    unsigned int nbits = width < 0 ? (unsigned int)-width : (unsigned int)width;
    unsigned int stored = mode == SZ_RAWBITS ? nbits : nbits <= 8 ? 8 : nbits <= 16 ? 16 : 32;
    uint32_t mask = nbits == 32 ? UINT32_MAX : (UINT32_C(1) << nbits) - 1;
    value &= mask;
    qbyte *data = malloc((stored + 7) / 8);
    assert(data);
    sizebuf_t msg = { .data=data, .maxsize=(int)((stored + 7) / 8), .packing=mode };
    memset(data, 0xa5, (size_t)msg.maxsize);
    MSG_WriteBits(&msg, (int)value, width);
    assert(!msg.overflowed && msg.cursize == msg.maxsize);
    msg.currentbit = 0;
    msg_readmsg = &msg;
    msg_badread = false;
    int result = MSG_ReadBits(width);
    int64_t expected = value;
    if (width < 0 && (value & (UINT32_C(1) << (nbits - 1))))
        expected -= INT64_C(1) << nbits;
    assert(!msg_badread && (uint32_t)result == (uint32_t)expected);
    assert(msg.currentbit == (int)stored);
    /* Exactly-sized packets must decode, then reject any additional field. */
    if (mode == SZ_RAWBITS) msg.currentbit = msg.cursize * 8;
    (void)MSG_ReadBits(8);
    assert(msg_badread);
    free(data);
}

static void sequence(void)
{
    qbyte data[512];
    sizebuf_t msg = { .data=data, .maxsize=sizeof(data), .packing=SZ_RAWBITS };
    uint32_t values[100]; int widths[100];
    for (int i=0; i<100; i++) {
        widths[i] = 1 + (int)(random_value() % 32);
        uint32_t mask = widths[i] == 32 ? UINT32_MAX : (UINT32_C(1) << widths[i]) - 1;
        values[i] = random_value() & mask;
        MSG_WriteBits(&msg, (int)values[i], widths[i]);
    }
    assert(!msg.overflowed);
    msg.currentbit=0; msg_readmsg=&msg; msg_badread=false;
    for (int i=0; i<100; i++) assert((uint32_t)MSG_ReadBits(widths[i]) == values[i]);
    assert(!msg_badread);
}

static void invalid_width(int bits)
{
    expected_error=1;
    if (!setjmp(failure)) { (void)MSG_ReadBits(bits); assert(!"invalid width accepted"); }
    expected_error=0;
}
static void invalid_space(sizebuf_t *msg,int length)
{
    expected_error=1;
    if (!setjmp(failure)) { (void)SZ_GetSpace(msg,length); assert(!"invalid allocation accepted"); }
    expected_error=0;
}

int main(void)
{
    for (int mode=SZ_RAWBYTES; mode<=SZ_RAWBITS; mode++)
        for (int width=1; width<=32; width++)
            for (int n=0; n<100; n++) {
                uint32_t v = n==0 ? UINT32_MAX : n==1 ? 0 : random_value();
                packet(mode, width, v);
                if (width<32) packet(mode, -width, v);
            }
    for (int i=0; i<100; i++) sequence();
    /* Every truncated byte-count, and each bit offset at the end of input. */
    for (int len=0; len<4; len++) {
        qbyte *data=malloc((size_t)len + 1);
        assert(data); memset(data, 0xff, (size_t)len + 1);
        for (int mode=SZ_RAWBYTES; mode<=SZ_RAWBITS; mode++) {
            sizebuf_t msg={.data=data,.maxsize=len,.cursize=len,.packing=mode};
            msg_readmsg=&msg; msg_badread=false;
            assert(MSG_ReadBits(32)==-1 && msg_badread);
            msg.currentbit=len*8; msg_badread=false;
            assert(MSG_ReadBits(1)==-1 && msg_badread);
        }
        free(data);
    }
    int invalid[]={0,-32,33,INT_MIN,INT_MAX};
    for (unsigned int i=0; i<sizeof(invalid)/sizeof(invalid[0]); i++) invalid_width(invalid[i]);
    qbyte data[1]={0xa5};
    sizebuf_t msg={.data=data,.maxsize=1,.packing=SZ_RAWBITS};
    MSG_WriteBits(&msg, 0xff, 8);
    MSG_WriteBits(&msg, 1, 1);
    assert(msg.overflowed && msg.cursize==1 && data[0]==0xff);
    msg.allowoverflow=true;
    invalid_space(&msg,2);
    invalid_space(&msg,-1);
    msg.cursize=1;
    assert(SZ_GetSpace(&msg,1)==data && msg.overflowed && msg.cursize==1);
    puts("Message fields: 12,600 signed/unsigned round trips, 10,000 unaligned fields, truncation and buffer overflow passed");
}
