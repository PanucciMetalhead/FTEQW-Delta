/* Cvar integer masks retain their bits without signed arithmetic overflow. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* ENGINE_FUNCTIONS */
int main(void)
{
    assert(Q_atoi("0x80000000")==INT_MIN);
    assert(Q_atoi("-2147483648")==INT_MIN);
    assert(Q_atoi("0xffffffff")==-1);
    assert(Q_atoi("4294967296")==0);
    assert(Q_atoi("0x100000000")==0);
    assert(Q_atoi("-0xffffffff")==1);
    assert(Q_atoi("'A")==65 && Q_atoi("-'A")==-65);
    assert(Q_atoi("")==0 && Q_atoi("hello")==0);
    /* Preserve the historic parser's whitespace/plus and trailing-text rules. */
    assert(Q_atoi(" 12")==0 && Q_atoi("+12")==0 && Q_atoi("12tail")==12);
    uint32_t state=0x98765432u;
    for (int i=0;i<100000;i++) {
        state=state*1664525u+1013904223u;
        char text[32]; snprintf(text,sizeof(text),"0x%08x",state);
        assert((uint32_t)Q_atoi(text)==state);
        snprintf(text,sizeof(text),"%u",state);
        assert((uint32_t)Q_atoi(text)==state);
        snprintf(text,sizeof(text),"-%u",state);
        assert((uint32_t)Q_atoi(text)==0u-state);
    }
    char longtext[4096];memset(longtext,'9',sizeof(longtext)-1);longtext[4095]=0;
    (void)Q_atoi(longtext);
    puts("Cvar numbers: 300,000 decimal/hex/sign conversions, full-width masks and long input passed");
}
