/* Exercise the actual console input capacity marker without drawing services. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define countof(a) (sizeof(a)/sizeof((a)[0]))
typedef uint32_t conchar_t;
#define CON_WHITEMASK 0x0f000000u
#define CON_NONCLEARBG 0x10000000u
static void check_capacity(unsigned length)
{
    conchar_t maskedtext[2048];
    for(unsigned i=0;i<countof(maskedtext);i++) maskedtext[i]=0xa5a5a5a5u;
    maskedtext[length]=0;
    conchar_t *endmtext=maskedtext+length;
    /* ENGINE_INPUT_CAPACITY */
    assert(maskedtext[length+1]==0);
    for(unsigned i=0;i<countof(maskedtext);i++) {
        if(i==length || i==length+1) assert(maskedtext[i]==0);
        else if(length==countof(maskedtext)-2 && i==length-1)
            assert(maskedtext[i]==(CON_WHITEMASK|'+'|CON_NONCLEARBG));
        else assert(maskedtext[i]==0xa5a5a5a5u);
    }
}
int main(void)
{
    for(unsigned i=0;i<=2046;i++) check_capacity(i);
    puts("Console input: empty through maximum input lengths preserve the capacity marker and both terminators without out-of-array pointer arithmetic");
    return 0;
}
