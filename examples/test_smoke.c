#define _DEFAULT_SOURCE
#define TEST_IMPLEMENTATION
#include "../test.h"

TEST(sanity) {
    ASSERT(1 + 1 == 2);
    ASSERT_STR_EQ("hello", "hello");
    ASSERT_STR_CONTAINS("hello world", "world");
    return TEST_OK;
}

int main(void) {
    const test_case cases[] = {
        TEST_ENTRY(sanity),
    };
    return test_run(cases, (int)(sizeof cases / sizeof cases[0]));
}
