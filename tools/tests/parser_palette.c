// Actual indexed-palette lookup builder with a controlled conversion service.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char qbyte;
static qbyte palette[256*3];
static qbyte *host_basepal=palette;
qbyte default_quakepal[256*3];
static unsigned int d_8to24rgbtable[256],d_8to24srgbtable[256],d_8to24bgrtable[256],d_quaketo24srgbtable[256];
// Conversion is a host service here; tests cover byte order and alpha packing.
static int SRGBb(int color) {return 255-color;}
#define bound(lo,v,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
static struct {float value;} gl_specular_fallbackexp;
/* ENGINE_FUNCTIONS */
int main(void) {
    for(int i=0;i<256;i++) {
        palette[3*i]=(qbyte)i;palette[3*i+1]=(qbyte)(255-i);palette[3*i+2]=(qbyte)(i^0x55);
        default_quakepal[3*i]=(qbyte)(i^0x33);default_quakepal[3*i+1]=(qbyte)i;default_quakepal[3*i+2]=(qbyte)(255-i);
    }
    palette[0]=0x11;palette[1]=0x22;palette[2]=0x33;
    R_GenPaletteLookup();
    assert(d_8to24rgbtable[0]==0xff332211u);
    assert(d_8to24bgrtable[0]==0xff112233u);
    assert(d_8to24srgbtable[0]==0xffccddeeu); // Converted 33/22/11 -> cc/dd/ee.
    for(int i=0;i<256;i++) {
        uint32_t rgb=d_8to24rgbtable[i],bgr=d_8to24bgrtable[i],srgb=d_8to24srgbtable[i];
        assert((rgb&255u)==palette[3*i] && ((rgb>>8)&255u)==palette[3*i+1] && ((rgb>>16)&255u)==palette[3*i+2]);
        assert((bgr&255u)==palette[3*i+2] && ((bgr>>16)&255u)==palette[3*i]);
        assert((srgb&255u)==(unsigned int)SRGBb(palette[3*i]));
        assert((rgb>>24)==(i==255?0u:255u) && (bgr>>24)==(rgb>>24) && (srgb>>24)==(rgb>>24));
        assert((d_quaketo24srgbtable[i]>>24)==255u);
        assert((d_quaketo24srgbtable[i]&255u)==(unsigned int)SRGBb(default_quakepal[3*i]));
    }
    const float values[]={-1,0,.5f,1,2};
    const uint32_t expected[]={0x00123456u,0x00123456u,0x7f123456u,0xff123456u,0xff123456u};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++) {
        gl_specular_fallbackexp.value=values[i];
        unsigned int glossval=0x00123456u;
        /* ENGINE_GLOSS_ALPHA */
        assert(glossval==expected[i]);
    }
    puts("Palette lookup: RGB/BGR byte order, conversion routing, opaque alpha and transparent index 255 passed");
    return 0;
}
