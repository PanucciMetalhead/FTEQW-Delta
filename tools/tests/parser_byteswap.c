/* Actual native engine byte swaps, including signed input bit patterns. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef int64_t qint64_t;
typedef uint64_t quint64_t;
/* ENGINE_FUNCTIONS */
static uint64_t reverse(uint64_t input, int bytes)
{
    uint64_t output = 0;
    for (int i = 0; i < bytes; ++i) { output = output * 256 + input % 256; input /= 256; }
    return output;
}
int main(void)
{
    for (unsigned int i = 0; i <= 65535; ++i)
    {
        unsigned short result = ShortSwap((short)i);
        assert(result == reverse(i,2) && (unsigned short)ShortSwap((short)result) == i);
    }
    uint64_t state = UINT64_C(0x8000000000000001);
    for (int i = 0; i < 100000; ++i)
    {
        state = state * UINT64_C(6364136223846793005) + 1;
        uint32_t value = state;
        uint32_t result = LongSwap((int)value);
        assert(result == reverse(value,4) && (uint32_t)LongSwap((int)result) == value);
        uint64_t wide = I64Swap((qint64_t)state);
        assert(wide == reverse(state,8) && (uint64_t)I64Swap((qint64_t)wide) == state);
    }
    assert((uint64_t)I64Swap((qint64_t)UINT64_C(0x8877665544332211)) == UINT64_C(0x1122334455667788));
    puts("Native byte swaps: every 16-bit pattern and 100,000 signed 32/64-bit patterns preserve reversed bytes");
    return 0;
}
