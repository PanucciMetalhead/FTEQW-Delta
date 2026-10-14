// Exercise real archive enumeration/sorting/loading with host services only.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#define QDECL
#define true 1
#define Q_strcasecmp strcasecmp
#define Z_Free free
typedef int64_t qofs_t;
typedef struct {int id;} searchpathfuncs_t;
typedef struct {int id;} wildpaks_t;
typedef struct {
    char *names;
    size_t numnames, maxnames;
    struct wilddatafile_s {
        size_t nameofs, size;
        time_t mtime;
        searchpathfuncs_t *source;
    } *files;
    size_t numfiles, maxfiles;
} filelist_t;
static struct {int ival;} dpcompat_ignoremodificationtimes;
static const char *qsortsucks;
static size_t sorts, loaded;
static char loaded_names[256][64];
static searchpathfuncs_t source;
static wildpaks_t loader;
static void Z_ReallocElements(void **p, size_t *capacity, size_t count, size_t bytes) {
    assert(count && bytes && count <= SIZE_MAX/bytes);
    void *next = realloc(*p, count*bytes); assert(next);
    *p = next; *capacity = count;
}
static void FS_AddSingleDataFile(const char *name, wildpaks_t *wp, searchpathfuncs_t *parent) {
    assert(wp == &loader && parent == &source && loaded < 256);
    assert(strlen(name) < sizeof(loaded_names[0]));
    strcpy(loaded_names[loaded++], name);
}
// Give the host qsort the same nonnull contract that Linux libc declares, so
// regressions are caught on macOS too. Sorting itself still uses the real libc.
static void checked_qsort(void *base, size_t count, size_t size,
                          int (*compare)(const void *, const void *)) __attribute__((nonnull(1,4)));
static void checked_qsort(void *base, size_t count, size_t size,
                          int (*compare)(const void *, const void *)) {
    assert(count > 1); sorts++; qsort(base, count, size, compare);
}
#define qsort checked_qsort
/* ENGINE_FUNCTIONS */
static void cleared(const filelist_t *list) {
    assert(!list->files && !list->names && !list->numfiles && !list->numnames);
    assert(!list->maxfiles && !list->maxnames && !qsortsucks);
}
static void scan(filelist_t *list, size_t count, int ignore_times) {
    dpcompat_ignoremodificationtimes.ival = ignore_times;
    loaded = sorts = 0;
    for (size_t i = count; i > 0; i--) {
        char name[64]; snprintf(name, sizeof(name), "archive%04zu.pk3", i);
        assert(FS_FindWildDataFiles(name, (qofs_t)i, ignore_times ? (time_t)i : 0, list, &source));
    }
    assert(list->numfiles == count);
    FS_LoadWildDataFiles(list, &loader);
    assert(loaded == count && sorts == (count > 1));
    for (size_t i = 0; i < count; i++) {
        char expected[64]; snprintf(expected, sizeof(expected), "archive%04zu.pk3", i+1);
        assert(!strcmp(loaded_names[i], expected));
    }
    cleared(list);
}
int main(void) {
    filelist_t list = {0};
    // Zero-result scans can occur repeatedly during a data-free startup.
    for (int i = 0; i < 8; i++) scan(&list, 0, 0);
    scan(&list, 1, 0);
    scan(&list, 2, 0);
    scan(&list, 140, 0); // Exercise enumeration-table growth.
    scan(&list, 140, 1); // The ignore-times option keeps the existing name order.
    // Release spare allocations even when a scan contains no actual files.
    list.names = malloc(16); list.files = malloc(sizeof(*list.files));
    assert(list.names && list.files); list.maxnames = 16; list.maxfiles = 1;
    scan(&list, 0, 0);
    scan(&list, 1, 0); // Reusing the cleared structure remains safe.
    puts("Archive scans: empty/single/multiple lists, ordering, allocation growth, cleanup and reuse passed");
    return 0;
}
