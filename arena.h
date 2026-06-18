/*
 * arena.h — stb
 *
 * Bump-pointer arena allocator: fast linear allocations, reset in O(1).
 *
 *   #define ARENA_IMPLEMENTATION
 *   #include "arena.h"
 *
 * Custom allocators (define before ARENA_IMPLEMENTATION):
 *
 *   #define ARENA_MALLOC(size) my_malloc(size)
 *   #define ARENA_FREE(ptr)    my_free(ptr)
 */

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct Arena {
    uint8_t *memory;
    size_t capacity;
    size_t used;
} Arena;

Arena arena_create(size_t capacity);
void arena_destroy(Arena *arena);
void arena_reset(Arena *arena);

void *arena_alloc(Arena *arena, size_t size);
void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);

char *arena_strdup(Arena *arena, const char *src);

#endif /* ARENA_H */

#ifdef ARENA_IMPLEMENTATION

#ifndef ARENA_MALLOC
#include <stdlib.h>
#define ARENA_MALLOC(size) malloc(size)
#define ARENA_FREE(ptr) free(ptr)
#endif

#include <string.h>

#ifndef ARENA_ASSERT
#include <assert.h>
#define ARENA_ASSERT(x) assert(x)
#endif

Arena arena_create(size_t capacity)
{
    Arena arena = {0};
    if (capacity == 0)
        return arena;
    arena.memory = (uint8_t *)ARENA_MALLOC(capacity);
    if (!arena.memory)
        return arena;
    arena.capacity = capacity;
    return arena;
}

void arena_destroy(Arena *arena)
{
    if (!arena)
        return;
    if (arena->memory)
        ARENA_FREE(arena->memory);
    *arena = (Arena){0};
}

void arena_reset(Arena *arena)
{
    if (arena)
        arena->used = 0;
}

static size_t arena__align_up(size_t value, size_t alignment)
{
    ARENA_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0);
    return (value + alignment - 1) & ~(alignment - 1);
}

void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment)
{
    size_t offset;
    void *ptr;

    if (!arena || !arena->memory || size == 0)
        return NULL;

    offset = arena__align_up(arena->used, alignment);
    if (offset > arena->capacity || size > arena->capacity - offset)
        return NULL;

    ptr = arena->memory + offset;
    arena->used = offset + size;
    return ptr;
}

void *arena_alloc(Arena *arena, size_t size)
{
    return arena_alloc_aligned(arena, size, sizeof(void *));
}

char *arena_strdup(Arena *arena, const char *src)
{
    size_t len;
    char *dst;

    if (!src)
        return NULL;
    len = strlen(src) + 1;
    dst = (char *)arena_alloc(arena, len);
    if (dst)
        memcpy(dst, src, len);
    return dst;
}

#endif /* ARENA_IMPLEMENTATION */

/*
 * arena.h — public domain / MIT
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
