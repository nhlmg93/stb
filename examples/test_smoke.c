#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <string.h>

static MunitResult test_sanity(const MunitParameter params[], void *user_data) {
    (void) params;
    (void) user_data;
    munit_assert_int(1 + 1, ==, 2);
    munit_assert_string_equal("hello", "hello");
    munit_assert_not_null(strstr("hello world", "world"));
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/sanity", test_sanity, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
