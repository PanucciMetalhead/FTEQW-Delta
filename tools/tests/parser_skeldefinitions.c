/* Production rig definition copying and cache ownership, independent of Jolt. */
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define countof(a) (sizeof(a) / sizeof((a)[0]))
typedef bool qboolean;
typedef struct doll_s doll_t;
typedef struct {
    doll_t *dollinfo;
} model_t;
struct doll_s {
    char *name, *definition;
    qboolean registered;
    int uses;
    model_t *model;
    doll_t *next;
    void *bone, *body, *joint;
};
typedef struct {
    doll_t *d;
    int errors;
} dollcreatectx_t;
static doll_t *dolllist;
typedef struct { doll_t *doll; } skeleton_t;
static skeleton_t skelobjects[4];
static int numskelobjectsused = 4;
static unsigned blocks, strings;
static const char *current;
static void *BZ_Malloc(size_t size) {
    void *p = calloc(1, size);
    assert(p);
    ++blocks;
    return p;
}
static void BZ_Free(void *p) {
    if (p) {
        assert(blocks);
        --blocks;
        free(p);
    }
}
static char *copy_string(const char *text) {
    assert(text);
    char *p = malloc(strlen(text) + 1);
    assert(p);
    strcpy(p, text);
    ++strings;
    return p;
}
static void free_string(void *p) {
    if (p) {
        assert(strings);
        --strings;
        free(p);
    }
}
void rag_freedoll(doll_t *);
static void rag_uninstanciate(void *value) {
    skeleton_t *s = value;
    if (s->doll) {
        assert(s->doll->uses);
        --s->doll->uses;
        s->doll = NULL;
    }
}
static void rag_uninstanciateall(void) {
    for (int i = 0; i < numskelobjectsused; i++)
        rag_uninstanciate(&skelobjects[i]);
}
static dollcreatectx_t *rag_createdoll(model_t *model, const char *name, int bones) {
    assert(bones == 3);
    dollcreatectx_t *ctx = BZ_Malloc(sizeof(*ctx));
    ctx->d = BZ_Malloc(sizeof(*ctx->d));
    ctx->d->name = copy_string(name);
    ctx->d->model = model;
    return ctx;
}
static doll_t *rag_finishdoll(dollcreatectx_t *ctx) {
    doll_t *d = ctx->d;
    int bad = ctx->errors;
    BZ_Free(ctx);
    if (bad) {
        rag_freedoll(d);
        return NULL;
    }
    return d;
}
static const char *Cmd_TokenizeString(const char *text, bool a, bool b) {
    current = text;
    const char *end = strchr(text, '\n');
    return end ? end + 1 : NULL;
}
static const char *Cmd_Argv(int n) {
    (void)n;
    return "bad";
}
static bool rag_dollline(dollcreatectx_t *ctx, int line) {
    (void)ctx;
    (void)line;
    return strncmp(current, "bad", 3) != 0;
}
static void Con_Printf(const char *format, ...) { (void)format; }
#define strdup copy_string
#define free free_string
/* ENGINE_FUNCTIONS */
int main(void) {
    model_t model = {0};
    for (int cycle = 0; cycle < 256; cycle++) {
        const char *definition = "valid one\nvalid two\n";
        doll_t *inline_doll = rag_createdollfromstring(&model, "", 3, definition);
        assert(inline_doll && !inline_doll->registered && !dolllist &&
               !strcmp(inline_doll->definition, definition));
        rag_cachedoll(inline_doll);
        rag_cachedoll(inline_doll);
        assert(dolllist == inline_doll && !inline_doll->next && inline_doll->registered);
        inline_doll->uses = 1;
        skelobjects[0].doll = inline_doll;
        doll_t *owned = rag_createdollfromstring(&model, "model-owned", 3, definition);
        assert(owned && !owned->registered && dolllist == inline_doll);
        model.dollinfo = owned;
        rag_freedoll(owned);
        assert(!model.dollinfo && dolllist == inline_doll);
        doll_t *restored = rag_createdollfromstring(&model, "<checkpoint>", 3, definition);
        rag_cachedoll(restored);
        assert(dolllist == restored && restored->next == inline_doll);
        rag_flushdolls(false);
        assert(dolllist == inline_doll && inline_doll->uses == 1);
        rag_freedoll(inline_doll);
        assert(!dolllist && !skelobjects[0].doll && !blocks && !strings);
        assert(!rag_createdollfromstring(&model, "invalid", 3, "bad\n") && !blocks && !strings);
        for (int i = 0; i < 4; i++) {
            doll_t *d = rag_createdollfromstring(&model, "<checkpoint>", 3, definition);
            rag_cachedoll(d);
            skelobjects[i].doll = d;
            d->uses = 1;
        }
        rag_flushdolls(false);
        assert(dolllist && blocks == 4);
        rag_flushdolls(true);
        assert(!dolllist && !blocks && !strings);
    }
    puts("Rig definitions: verbatim text, separate model/main-thread ownership, duplicate "
         "registration, unlink/flush, active cleanup and failed parse lifetime passed");
    return 0;
}
