/*
 * test.h — stb
 *
 * Minimal test harness for examples and single-file tests.
 *
 *   #define TEST_IMPLEMENTATION
 *   #include "test.h"
 *
 *   TEST(sanity) {
 *       ASSERT(1 + 1 == 2);
 *   }
 *
 *   int main(void) {
 *       const test_case cases[] = {
 *           TEST_ENTRY(sanity),
 *       };
 *       return test_run(cases, (int)(sizeof cases / sizeof cases[0]));
 *   }
 */

#ifndef TEST_H
#define TEST_H

typedef enum test_result {
    TEST_OK = 0,
    TEST_FAIL = 1,
    TEST_SKIP = 2
} test_result;

typedef struct test_case {
    const char *name;
    test_result (*fn)(void);
} test_case;

int test_run(const test_case *cases, int count);

#define TEST(name) static test_result test_##name(void)

#define TEST_ENTRY(name) {#name, test_##name}

#define ASSERT(cond)                                                                               \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            test_fail_at(__FILE__, __LINE__, #cond);                                               \
            return TEST_FAIL;                                                                      \
        }                                                                                          \
    } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(test_str_eq((a), (b)))
#define ASSERT_STR_CONTAINS(haystack, needle) ASSERT(test_str_contains((haystack), (needle)))

#define SKIP(msg)                                                                                  \
    do {                                                                                           \
        test_skip_msg(msg);                                                                        \
        return TEST_SKIP;                                                                          \
    } while (0)

#define SKIP_IF(cond, msg)                                                                         \
    do {                                                                                           \
        if (cond) {                                                                                \
            test_skip_msg(msg);                                                                    \
            return TEST_SKIP;                                                                      \
        }                                                                                          \
    } while (0)

void test_fail_at(const char *file, int line, const char *expr);
void test_skip_msg(const char *msg);
int test_str_eq(const char *a, const char *b);
int test_str_contains(const char *haystack, const char *needle);

#endif /* TEST_H */

#ifdef TEST_IMPLEMENTATION

#ifndef TEST_MAIN
#include <stdio.h>
#endif
#include <string.h>

static const char *test_skip_reason;

void test_fail_at(const char *file, int line, const char *expr) {
    fprintf(stderr, "  assert failed: %s (%s:%d)\n", expr, file, line);
}

void test_skip_msg(const char *msg) {
    test_skip_reason = msg;
}

int test_str_eq(const char *a, const char *b) {
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

int test_str_contains(const char *haystack, const char *needle) {
    size_t n;
    size_t hlen;
    size_t i;
    if (!haystack || !needle) return 0;
    if (!*needle) return 1;
    n = strlen(needle);
    hlen = strlen(haystack);
    if (hlen < n) return 0;
    for (i = 0; i + n <= hlen; i++) {
        if (memcmp(haystack + i, needle, n) == 0) return 1;
    }
    return 0;
}

int test_run(const test_case *cases, int count) {
    int i;
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    for (i = 0; i < count; i++) {
        test_result r;
        test_skip_reason = NULL;
        printf("[test] %s ... ", cases[i].name);
        fflush(stdout);
        r = cases[i].fn();
        if (r == TEST_OK) {
            printf("ok\n");
            passed++;
        } else if (r == TEST_SKIP) {
            printf("skip");
            if (test_skip_reason) printf(" (%s)", test_skip_reason);
            printf("\n");
            skipped++;
        } else {
            printf("FAIL\n");
            failed++;
        }
    }

    printf("\n%d passed, %d failed, %d skipped\n", passed, failed, skipped);
    return failed ? 1 : 0;
}

#endif /* TEST_IMPLEMENTATION */

/*
 * test.h — MIT License
 *
 * Copyright (c) 2026 stb contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
