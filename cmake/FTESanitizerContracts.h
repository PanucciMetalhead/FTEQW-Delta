/* Match glibc's pointer contracts in Darwin C sanitizer builds. No wrappers,
 * replacement implementations or changes to ordinary builds are involved. */
#ifndef FTE_SANITIZER_CONTRACTS_H
#define FTE_SANITIZER_CONTRACTS_H
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__) && !defined(__cplusplus) && (defined(__clang__) || defined(__GNUC__))
extern void *memcpy(void *, const void *, size_t) __attribute__((nonnull(1, 2)));
extern void *memmove(void *, const void *, size_t) __attribute__((nonnull(1, 2)));
extern void *memset(void *, int, size_t) __attribute__((nonnull(1)));
extern int memcmp(const void *, const void *, size_t) __attribute__((nonnull(1, 2)));
extern void qsort(void *, size_t, size_t, int (*)(const void *, const void *))
    __attribute__((nonnull(1, 4)));
#endif
#endif
