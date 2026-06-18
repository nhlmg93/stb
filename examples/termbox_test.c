#define MUNIT_NO_FORK
#define _DEFAULT_SOURCE
#define TB_OPT_ATTR_W 32
#define TB_IMPL
#include "../termbox2.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <unistd.h>

static MunitResult test_init_and_size(const MunitParameter params[], void *user_data) {
    int w;
    int h;

    (void) params;
    (void) user_data;

    if (!isatty(STDOUT_FILENO)) return MUNIT_SKIP;

    munit_assert_int(tb_init(), ==, TB_OK);
    w = tb_width();
    h = tb_height();
    munit_assert_true(w > 0);
    munit_assert_true(h > 0);
    tb_shutdown();
    return MUNIT_OK;
}

static MunitResult test_set_cell_and_present(const MunitParameter params[], void *user_data) {
    (void) params;
    (void) user_data;

    if (!isatty(STDOUT_FILENO)) return MUNIT_SKIP;

    munit_assert_int(tb_init(), ==, TB_OK);
    tb_clear();
    tb_set_cell(0, 0, 'X', TB_WHITE, TB_BLUE);
    tb_present();
    tb_shutdown();
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/init_and_size", test_init_and_size, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/set_cell_and_present", test_set_cell_and_present, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/termbox", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
