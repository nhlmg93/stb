#define JSON_IMPLEMENTATION
#include "../json.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <stdlib.h>
#include <string.h>

static MunitResult test_parse_object(const MunitParameter params[], void *user_data) {
    const char *src = "{\"name\":\"stb\",\"count\":2,\"ok\":true}";
    struct json_value_s *root = json_parse(src, strlen(src));
    struct json_object_s *obj;
    struct json_object_element_s *elem;
    struct json_string_s *name;
    struct json_number_s *count;

    (void) params;
    (void) user_data;

    munit_assert_not_null(root);
    munit_assert_int((int)root->type, ==, (int)json_type_object);

    obj = json_value_as_object(root);
    munit_assert_not_null(obj);

    elem = obj->start;
    munit_assert_not_null(elem);
    name = json_value_as_string(elem->value);
    munit_assert_not_null(name);
    munit_assert_int((int)name->string_size, ==, 3);
    munit_assert_int(memcmp(name->string, "stb", 3), ==, 0);

    elem = elem->next;
    munit_assert_not_null(elem);
    count = json_value_as_number(elem->value);
    munit_assert_not_null(count);
    munit_assert_string_equal(count->number, "2");

    elem = elem->next;
    munit_assert_not_null(elem);
    munit_assert_true(json_value_is_true(elem->value));

    free(root);
    return MUNIT_OK;
}

static MunitResult test_parse_array(const MunitParameter params[], void *user_data) {
    const char *src = "[1,2,3]";
    struct json_value_s *root = json_parse(src, strlen(src));
    struct json_array_s *arr;
    struct json_array_element_s *elem;
    int n = 0;

    (void) params;
    (void) user_data;

    munit_assert_not_null(root);
    arr = json_value_as_array(root);
    munit_assert_not_null(arr);

    for (elem = arr->start; elem; elem = elem->next) {
        munit_assert_not_null(json_value_as_number(elem->value));
        n++;
    }
    munit_assert_int(n, ==, 3);

    free(root);
    return MUNIT_OK;
}

static MunitResult test_parse_invalid(const MunitParameter params[], void *user_data) {
    const char *src = "{not json}";
    struct json_parse_result_s result;
    struct json_value_s *root =
        json_parse_ex(src, strlen(src), json_parse_flags_default, NULL, NULL, &result);

    (void) params;
    (void) user_data;

    munit_assert_null(root);
    munit_assert_int((int)result.error, !=, (int)json_parse_error_none);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/parse_object", test_parse_object, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/parse_array", test_parse_array, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/parse_invalid", test_parse_invalid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/json", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
