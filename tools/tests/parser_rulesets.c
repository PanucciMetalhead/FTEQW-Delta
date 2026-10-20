/* Real ruleset sorting must accept absent rule or file-hash lists. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define QDECL
#define Q_strcmp strcmp
typedef int hashfunc_t, qboolean;
typedef uint8_t qbyte;
/* ENGINE_RULESET_TYPES */
static void checked_qsort(void *base, size_t count, size_t size,
                          int (*compare)(const void *, const void *))
{
    assert(base && compare && count > 1);
    qsort(base, count, size, compare);
}
#define qsort checked_qsort
/* ENGINE_FUNCTIONS */
static void sort(ruleset_t *rs)
{
    /* ENGINE_RULESET_SORT */
}
int main(void)
{
    for (size_t rules = 0; rules <= 3; ++rules)
    for (size_t files = 0; files <= 3; ++files)
    {
        rulesetrule_t rule[] = {{"", "z", "3"}, {"", "m", "2"}, {"", "a", "1"}};
        rulesetfilehashes_t hashes[] = {{.filename="z.dat"}, {.filename="m.dat"}, {.filename="a.dat"}};
        ruleset_t rs = {.rules=rules, .rule=rules ? rule : NULL,
                        .filehashes=files, .filehash=files ? hashes : NULL};
        sort(&rs);
        assert(rs.rules == rules && rs.filehashes == files);
        for (size_t i = 1; i < rules; ++i) assert(strcmp(rule[i-1].rulename, rule[i].rulename) < 0);
        for (size_t i = 1; i < files; ++i) assert(strcmp(hashes[i-1].filename, hashes[i].filename) < 0);
        if (!rules) assert(!rs.rule);
        if (!files) assert(!rs.filehash);
    }
    puts("Rulesets: absent/empty, single and multiple rule/file lists sort consistently without null qsort arguments");
    return 0;
}
