#define ARENA_IMPLEMENTATION
#include "../arena.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <stdint.h>
#include <string.h>

static MunitResult test_create_and_alloc(const MunitParameter params[], void *user_data) {
    Arena arena = arena_create(256);
    int *a;
    int *b;

    (void)params;
    (void)user_data;

    munit_assert_not_null(arena.memory);
    munit_assert_size(arena.capacity, ==, 256);
    munit_assert_size(arena.used, ==, 0);

    a = (int *)arena_alloc(&arena, sizeof *a);
    b = (int *)arena_alloc(&arena, sizeof *b);
    munit_assert_not_null(a);
    munit_assert_not_null(b);
    munit_assert_ptr_not_equal(a, b);
    munit_assert_true((uint8_t *)b >= (uint8_t *)a + sizeof *a);

    *a = 42;
    *b = 99;
    munit_assert_int(*a, ==, 42);
    munit_assert_int(*b, ==, 99);

    arena_destroy(&arena);
    munit_assert_null(arena.memory);
    return MUNIT_OK;
}

static MunitResult test_reset(const MunitParameter params[], void *user_data) {
    Arena arena = arena_create(128);
    void *first;

    (void)params;
    (void)user_data;

    first = arena_alloc(&arena, 32);
    munit_assert_not_null(first);
    munit_assert_size(arena.used, >, 0);

    arena_reset(&arena);
    munit_assert_size(arena.used, ==, 0);
    munit_assert_ptr_equal(arena_alloc(&arena, 32), first);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_aligned_alloc(const MunitParameter params[], void *user_data) {
    Arena arena = arena_create(256);
    void *p;

    (void)params;
    (void)user_data;

    munit_assert_not_null(arena_alloc(&arena, 1));
    p = arena_alloc_aligned(&arena, 16, 64);
    munit_assert_not_null(p);
    munit_assert_int((int)(((uintptr_t)p - (uintptr_t)arena.memory) % 64), ==, 0);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_strdup(const MunitParameter params[], void *user_data) {
    Arena arena = arena_create(64);
    char *copy;

    (void)params;
    (void)user_data;

    copy = arena_strdup(&arena, "hello");
    munit_assert_not_null(copy);
    munit_assert_string_equal(copy, "hello");
    munit_assert_ptr_not_equal(copy, (void *)"hello");

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_out_of_memory(const MunitParameter params[], void *user_data) {
    Arena arena = arena_create(16);
    void *p;

    (void)params;
    (void)user_data;

    p = arena_alloc(&arena, 12);
    munit_assert_not_null(p);
    munit_assert_null(arena_alloc(&arena, 8));

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/create_and_alloc", test_create_and_alloc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/reset", test_reset, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/aligned_alloc", test_aligned_alloc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/strdup", test_strdup, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/out_of_memory", test_out_of_memory, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/arena", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
