#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

#define EPRINT(...) do { fprintf(stderr, "%s: ", __func__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)

static inline void *
malloc_or_exit(size_t size, const char *calling_function)
{
    void *mem = malloc(size);
    if (!mem) {
        fprintf(stderr, "%s: malloc() failed\n", calling_function);
        exit(EXIT_FAILURE);
    }
    return mem;
}
#define emalloc(size) malloc_or_exit(size, __func__)

static inline void *
calloc_or_exit(size_t num, size_t size, const char *calling_function)
{
    void *mem = calloc(num, size);
    if (!mem) {
        fprintf(stderr, "%s: calloc() failed\n", calling_function);
        exit(EXIT_FAILURE);
    }
    return mem;
}
#define ecalloc(num, size) calloc_or_exit(num, size, __func__)

#endif /* UTILS_H */
