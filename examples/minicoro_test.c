#define MINICORO_IMPL
#include "../minicoro.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

static int g_step;

static void coro_entry(mco_coro *co) {
    g_step = 1;
    mco_yield(co);
    g_step = 2;
}

static MunitResult test_yield_resume(const MunitParameter params[], void *user_data) {
    mco_desc desc = mco_desc_init(coro_entry, 0);
    mco_coro *co;
    mco_result res;

    (void) params;
    (void) user_data;

    g_step = 0;
    res = mco_create(&co, &desc);
    munit_assert_int((int)res, ==, (int)MCO_SUCCESS);
    munit_assert_int((int)mco_status(co), ==, (int)MCO_SUSPENDED);

    res = mco_resume(co);
    munit_assert_int((int)res, ==, (int)MCO_SUCCESS);
    munit_assert_int(g_step, ==, 1);
    munit_assert_int((int)mco_status(co), ==, (int)MCO_SUSPENDED);

    res = mco_resume(co);
    munit_assert_int((int)res, ==, (int)MCO_SUCCESS);
    munit_assert_int(g_step, ==, 2);
    munit_assert_int((int)mco_status(co), ==, (int)MCO_DEAD);

    res = mco_destroy(co);
    munit_assert_int((int)res, ==, (int)MCO_SUCCESS);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/yield_resume", test_yield_resume, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/minicoro", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
