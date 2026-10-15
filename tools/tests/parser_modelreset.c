/* Production model-worker reset with a synthetic model header and payload. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    char publicname[27];
    void *cache;
    unsigned char header[13];
    int loadstate;
    float mins[3], maxs[3];
    unsigned char payload[87];
} model_t;
static void reset(model_t *mod)
{
    /* ENGINE_MODEL_RESET */
}
int main(void)
{
    int cache;
    for (int iteration = 0; iteration < 256; ++iteration)
    {
        model_t model;
        memset(&model, 0xa5, sizeof(model));
        strcpy(model.publicname, "preserved-name");
        model.cache = &cache;
        model.loadstate = iteration;
        reset(&model);
        assert(!strcmp(model.publicname, "preserved-name"));
        assert(model.cache == &cache && model.loadstate == iteration);
        for (size_t i = 0; i < sizeof(model.header); ++i) assert(model.header[i] == 0xa5);
        for (int i = 0; i < 3; ++i) assert(model.mins[i] == 0 && model.maxs[i] == 0);
        for (size_t i = 0; i < sizeof(model.payload); ++i) assert(model.payload[i] == 0);
        reset(&model);
        assert(model.cache == &cache && model.loadstate == iteration);
    }
    puts("Model worker reset: header/name/cache/load state preserved; stale bounds and payload cleared");
    return 0;
}
