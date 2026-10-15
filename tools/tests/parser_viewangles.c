/* Actual client entity view-angle quantizer, including its protocol scale. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
/* ENGINE_ANGLE_SCALE */
/* ENGINE_FUNCTIONS */
int main(void) {
    for(int i=-196608;i<=196608;i++) {
        float angle=(float)(i*(360.0/65536.0));
        int reference=(int)(angle*(65536.0/360.0));
        assert((uint16_t)CLFTE_EncodeViewAngle(angle)==(uint16_t)reference);
    }
    const float values[]={179.999f,180,180.001f,-179.999f,-180,-180.001f,359.999f,360,-360,720,-720,540,-540};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++) {
        int reference=(int)(values[i]*(65536.0/360.0));
        assert((uint16_t)CLFTE_EncodeViewAngle(values[i])==(uint16_t)reference);
    }
    assert(CLFTE_EncodeViewAngle(180)==-32768 && CLFTE_EncodeViewAngle(270)==-16384);
    assert(CLFTE_EncodeViewAngle(NAN)==0 && CLFTE_EncodeViewAngle(INFINITY)==0);
    assert(CLFTE_EncodeViewAngle(-INFINITY)==0);
    (void)CLFTE_EncodeViewAngle(1e30f);(void)CLFTE_EncodeViewAngle(-1e30f);
    puts("Client entity angles: all 16-bit patterns over six rotations, fractional half-turn boundaries and nonfinite/oversized inputs avoid short overflow and preserve packet bits");
}
