#define CLAY_IMPLEMENTATION
#include "../clay.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <stdlib.h>

typedef struct {
    Clay_Arena arena;
    float measure_cell;
} clay_fixture;

static void on_clay_error(Clay_ErrorData err) {
    (void)err;
}

static Clay_Dimensions measure_stub(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Clay_Dimensions size = {0};
    float cell = *(float *)userData;
    (void)config;
    size.width = (float)text.length * cell;
    size.height = cell;
    return size;
}

static void *clay_setup(const MunitParameter params[], void *user_data) {
    clay_fixture *f;
    Clay_Dimensions dims = {640, 480};
    uint64_t mem_size;

    (void)params;
    (void)user_data;

    f = (clay_fixture *)munit_malloc(sizeof *f);
    f->measure_cell = 8.0f;
    mem_size = Clay_MinMemorySize();
    f->arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc((size_t)mem_size));
    Clay_Initialize(f->arena, dims, (Clay_ErrorHandler){ .errorHandlerFunction = on_clay_error });
    Clay_SetMeasureTextFunction(measure_stub, &f->measure_cell);
    return f;
}

static void clay_tear_down(void *fixture) {
    clay_fixture *f = fixture;
    free(f->arena.memory);
    free(f);
}

static Clay_RenderCommandArray build_ui(void) {
    Clay_BeginLayout();
    CLAY(CLAY_ID("Root"), { .layout = { .sizing = { CLAY_SIZING_GROW(0, 0), CLAY_SIZING_GROW(0, 0) } } }) {
        CLAY(CLAY_ID("Row"),
             { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT,
                           .childGap = 8,
                           .padding = CLAY_PADDING_ALL(4),
                           .sizing = { CLAY_SIZING_GROW(0, 0), CLAY_SIZING_FIXED(48) } },
               .backgroundColor = {30, 30, 40, 255} }) {
            CLAY_TEXT(CLAY_STRING("A"), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = {255, 255, 255, 255} }));
            CLAY_TEXT(CLAY_STRING("B"), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = {255, 255, 255, 255} }));
        }
    }
    return Clay_EndLayout(0);
}

static int count_commands(Clay_RenderCommandArray cmds, Clay_RenderCommandType type) {
    int n = 0;
    int i;
    for (i = 0; i < cmds.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
        if (cmd->commandType == type) n++;
    }
    return n;
}

static MunitResult test_min_memory_size(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;
    munit_assert_true(Clay_MinMemorySize() > 0);
    return MUNIT_OK;
}

static MunitResult test_element_ids(const MunitParameter params[], void *user_data) {
    Clay_ElementId a = CLAY_ID("A");
    Clay_ElementId b = CLAY_ID("B");

    (void)params;
    (void)user_data;

    munit_assert_int((int)a.id, !=, 0);
    munit_assert_int((int)b.id, !=, 0);
    munit_assert_int((int)a.id, !=, (int)b.id);
    return MUNIT_OK;
}

static MunitResult test_layout_commands(const MunitParameter params[], void *fixture) {
    Clay_RenderCommandArray cmds = build_ui();

    (void)params;
    (void)fixture;

    munit_assert_true(cmds.length > 0);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_TEXT) >= 2);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_RECTANGLE) >= 1);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/min_memory_size", test_min_memory_size, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/element_ids", test_element_ids, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/layout_commands", test_layout_commands, clay_setup, clay_tear_down, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/clay", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
