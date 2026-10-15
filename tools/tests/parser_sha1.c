// Real SHA-1/HMAC functions, checked against standard-library reference vectors.
#include <alloca.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char qbyte;
typedef struct {
    unsigned int digestsize, contextsize;
    void (*init)(void *);
    void (*process)(void *,const void *,size_t);
    void (*terminate)(unsigned char *,void *);
} hashfunc_t;
/* ENGINE_SHA1_TYPES */
/* ENGINE_FUNCTIONS */
static hashfunc_t sha1={20,sizeof(SHA1_CTX),SHA1Init,SHA1Update,SHA1Final};
static void check_hex(unsigned char *digest,const char *expected) {
    const char hex[]="0123456789abcdef";char text[41];
    for(int i=0;i<20;i++) {text[i*2]=hex[digest[i]>>4];text[i*2+1]=hex[digest[i]&15];}
    text[40]=0;assert(!strcmp(text,expected));
}
static unsigned char *bytes(size_t size,uint32_t seed) {
    unsigned char *data=malloc(size?size:1);assert(data);
    for(size_t i=0;i<size;i++) {seed=seed*1664525u+1013904223u;data[i]=(unsigned char)(seed>>24);}
    return data;
}
struct digestcase {size_t size;uint32_t seed;const char *expected;};
static const struct digestcase cases[]={
/* SHA1_REFERENCE_CASES */
};
struct hmaccase {size_t keysize;const char *expected;};
static const struct hmaccase hmac_cases[]={
/* HMAC_REFERENCE_CASES */
};
int main(void) {
    unsigned char digest[20];
    assert(CalcHash(&sha1,digest,sizeof(digest),(const unsigned char *)"abc",3)==20);
    check_hex(digest,"a9993e364706816aba3e25717850c26c9cd0d89d");
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        unsigned char *data=bytes(cases[i].size,cases[i].seed);
        assert(CalcHash(&sha1,digest,sizeof(digest),data,cases[i].size)==20);
        check_hex(digest,cases[i].expected);
        SHA1_CTX ctx;SHA1Init(&ctx);
        for(size_t offset=0;offset<cases[i].size;) {
            size_t n=cases[i].size-offset;if(n>7)n=7;
            SHA1Update(&ctx,data+offset,n);offset+=n;
        }
        SHA1Final(digest,&ctx);check_hex(digest,cases[i].expected);free(data);
    }
    unsigned char *data=bytes(127,42);
    for(size_t i=0;i<sizeof(hmac_cases)/sizeof(hmac_cases[0]);i++) {
        unsigned char *key=bytes(hmac_cases[i].keysize,99);
        assert(CalcHMAC(&sha1,digest,sizeof(digest),data,127,key,hmac_cases[i].keysize)==20);
        check_hex(digest,hmac_cases[i].expected);free(key);
    }
    free(data);
    puts("SHA-1/HMAC: canonical abc, block/padding boundaries, million-byte input, streaming and long keys match Python hashlib/hmac");
    return 0;
}
