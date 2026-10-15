/* Actual worker GC snapshot layout: VM words followed by native pointers. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uintptr_t quintptr_t;
enum {false, true};
typedef struct { int value; } tempstr_t;
typedef struct {
    struct {
        unsigned int addressableused, maxtempstrings;
        unsigned int *addressablehunk;
        tempstr_t **tempstrings;
    } state;
} progfuncs_t;
#define prinst (progfuncs->state)
/* ENGINE_GC_CONTEXT */
/* ENGINE_FUNCTIONS */
int main(void)
{
    progfuncs_t instance = {0};
    progfuncs_t *progfuncs = &instance;
    unsigned int memory[513];
    tempstr_t strings[33], *pointers[33];
    for (int i = 0; i < 513; ++i) memory[i] = 0x80000000u + i;
    for (int i = 0; i < 33; ++i) {strings[i].value=i; pointers[i]=&strings[i];}
    prinst.addressablehunk = memory;
    prinst.tempstrings = pointers;
    for (unsigned int size = 0; size <= sizeof(memory); size += 4)
    for (unsigned int count = 0; count <= 33; ++count)
    {
        prinst.addressableused = size;
        prinst.maxtempstrings = count;
        struct qcgccontext_s *gc = PR_QCGC_Snapshot(progfuncs);
        assert(gc->progfuncs == progfuncs && !gc->done && gc->clearedtemps == 0);
        assert(gc->memsize == size && gc->maxtemps == count);
        assert((uintptr_t)gc->tempstrings % _Alignof(tempstr_t*) == 0);
        assert((unsigned char *)gc->tempstrings >= (unsigned char *)gc->amem + size);
        assert(!memcmp(gc->amem, memory, size));
        for (unsigned int i = 0; i < count; ++i)
            assert(gc->tempstrings[i] == pointers[i] && gc->tempstrings[i]->value == (int)i);
        free(gc);
    }
    puts("Worker GC snapshots: word-sized VM memory and native pointer tables stay aligned and preserve contents");
    return 0;
}
