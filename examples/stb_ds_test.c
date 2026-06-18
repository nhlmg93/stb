#define STB_DS_IMPLEMENTATION
#include "../stb_ds.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

typedef struct {
    int key;
    const char *value;
} map_entry_t;

static MunitResult test_dynamic_array(const MunitParameter params[], void *user_data) {
    int *nums = NULL;
    int i;

    (void) params;
    (void) user_data;

    arrpush(nums, 10);
    arrpush(nums, 20);
    arrpush(nums, 30);
    munit_assert_int((int)arrlen(nums), ==, 3);
    munit_assert_int(nums[0], ==, 10);
    munit_assert_int(nums[2], ==, 30);

    arrdel(nums, 1);
    munit_assert_int((int)arrlen(nums), ==, 2);
    munit_assert_int(nums[0], ==, 10);
    munit_assert_int(nums[1], ==, 30);

    for (i = 0; i < arrlen(nums); i++) nums[i] *= 2;
    munit_assert_int(nums[1], ==, 60);

    arrfree(nums);
    return MUNIT_OK;
}

static MunitResult test_hash_map(const MunitParameter params[], void *user_data) {
    map_entry_t *items = NULL;
    const char *found;

    (void) params;
    (void) user_data;

    hmput(items, 1, "alpha");
    hmput(items, 2, "beta");
    munit_assert_int((int)hmlen(items), ==, 2);

    found = hmget(items, 2);
    munit_assert_string_equal(found, "beta");

    hmdel(items, 1);
    munit_assert_int((int)hmlen(items), ==, 1);
    munit_assert_true(hmgeti(items, 1) < 0);

    hmfree(items);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/dynamic_array", test_dynamic_array, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/hash_map", test_hash_map, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/stb_ds", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
