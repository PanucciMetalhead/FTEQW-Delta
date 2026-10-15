// Archive-purity hash folding through the actual engine function.
#include <alloca.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    unsigned int digestsize, contextsize;
    void (*init)(void *);
    void (*process)(void *,const void *,size_t);
    void (*terminate)(unsigned char *,void *);
} hashfunc_t;
static size_t digest_size;
static void terminate(unsigned char *out,void *context) {memcpy(out,context,digest_size);}
/* ENGINE_ZIP_ENDIAN */
/* ENGINE_FUNCTIONS */
static unsigned int fold(unsigned char *data,size_t n) {
    hashfunc_t func={0};digest_size=n;func.digestsize=(unsigned int)n;func.terminate=terminate;
    return hashfunc_terminate_uint(&func,data);
}
int main(void) {
    unsigned char bytes[64]={0x11,0x22,0x33,0xff};
    assert(sizeof(unsigned int)==4);
    assert(fold(bytes,4)==0xff332211u);
    assert(fold(bytes,3)==0x00332211u);
    memset(bytes,0xff,sizeof(bytes));
    assert(fold(bytes,4)==0xffffffffu);
    assert(fold(bytes,8)==0);
    for(unsigned int v=0;v<256;v++) {
        memset(bytes,0,sizeof(bytes));bytes[3]=(unsigned char)v;
        assert(fold(bytes,4)==v*0x1000000u);
    }
    uint32_t random=0x12345678;
    for(size_t words=1;words<=16;words++) {
        for(int trial=0;trial<1000;trial++) {
            uint32_t expected=0;
            for(size_t i=0;i<words;i++) {
                random=random*1664525u+1013904223u;expected^=random;
                bytes[4*i]=(unsigned char)random;bytes[4*i+1]=(unsigned char)(random>>8);
                bytes[4*i+2]=(unsigned char)(random>>16);bytes[4*i+3]=(unsigned char)(random>>24);
            }
            assert(fold(bytes,words*4)==expected);
        }
    }
    for(int i=0;i<100000;i++) {
        random=random*1664525u+1013904223u;
        bytes[0]=(unsigned char)random;bytes[1]=(unsigned char)(random>>8);
        bytes[2]=(unsigned char)(random>>16);bytes[3]=(unsigned char)(random>>24);
        assert(LittleU4FromPtr(bytes)==random);
        struct {unsigned char in[8];} trailer={{0}}, *n=&trailer;
        memcpy(n->in+4,bytes,4);
        /* ENGINE_GZIP_SIZE */
        assert(isize==random);
    }
    puts("ZIP/GZIP byte-order decoding: 100,000 complete words preserve unsigned fields");
    puts("Hash folding: known checksum words, every high byte and 16,000 digest patterns preserve unsigned bit patterns");
    return 0;
}
