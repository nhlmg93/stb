#define CLAY_IMPLEMENTATION
#define CLAY_RAYLIB_IMPLEMENTATION
#include "../clay.h"
#include "../clay_raylib.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    Clay_Arena arena;
    Font font;
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
    static float measure_cell = 8.0f;

    (void)params;
    (void)user_data;

    f = (clay_fixture *)munit_malloc(sizeof *f);
    mem_size = Clay_MinMemorySize();
    f->arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc((size_t)mem_size));
    Clay_Initialize(f->arena, dims, (Clay_ErrorHandler){ .errorHandlerFunction = on_clay_error });
    Clay_SetMeasureTextFunction(measure_stub, &measure_cell);
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
        CLAY(CLAY_ID("Panel"),
             { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM,
                           .padding = CLAY_PADDING_ALL(16),
                           .childGap = 12,
                           .sizing = { CLAY_SIZING_GROW(0, 0), CLAY_SIZING_GROW(0, 0) } },
               .backgroundColor = {44, 48, 58, 255},
               .cornerRadius = CLAY_CORNER_RADIUS(8),
               .border = { .width = {2, 2, 2, 2, 0}, .color = {90, 100, 120, 255} } }) {
            CLAY_TEXT(CLAY_STRING("Clay raylib test"), CLAY_TEXT_CONFIG({ .fontSize = 32, .textColor = {240, 240, 240, 255} }));
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

static int has_display(void) {
    return getenv("DISPLAY") != NULL || getenv("WAYLAND_DISPLAY") != NULL;
}

static int raylib_test_begin(clay_fixture *f) {
    if (!has_display()) return 0;
    Clay_Raylib_Initialize(640, 480, "clay_raylib test", FLAG_WINDOW_HIDDEN);
    f->font = GetFontDefault();
    Clay_SetMeasureTextFunction(Clay_Raylib_MeasureText, &f->font);
    return 1;
}

static void raylib_test_end(void) {
    Clay_Raylib_Close();
}

static MunitResult test_measure_text(const MunitParameter params[], void *fixture) {
    clay_fixture *f = fixture;
    Clay_TextElementConfig cfg = { .fontSize = 32 };
    static const char label[] = "Clay raylib test";
    Clay_StringSlice text = { .length = (int32_t)(sizeof label - 1), .chars = label, .baseChars = label };
    Clay_Dimensions dims;

    (void)params;

    if (!raylib_test_begin(f)) return MUNIT_SKIP;

    dims = Clay_Raylib_MeasureText(text, &cfg, &f->font);
    munit_assert_true(dims.width > 0);
    munit_assert_true(dims.height > 0);
    raylib_test_end();
    return MUNIT_OK;
}

static MunitResult test_layout_commands(const MunitParameter params[], void *fixture) {
    Clay_RenderCommandArray cmds = build_ui();

    (void)params;
    (void)fixture;

    munit_assert_true(cmds.length > 0);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_TEXT) >= 1);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_RECTANGLE) >= 1);
    return MUNIT_OK;
}

static MunitResult test_render_frame(const MunitParameter params[], void *fixture) {
    clay_fixture *f = fixture;
    Clay_RenderCommandArray cmds;
    Font fonts[1];

    (void)params;

    if (!raylib_test_begin(f)) return MUNIT_SKIP;

    fonts[0] = f->font;
    BeginDrawing();
    ClearBackground((Color){20, 20, 28, 255});
    cmds = build_ui();
    Clay_Raylib_Render(cmds, fonts);
    EndDrawing();
    munit_assert_true(IsWindowReady());
    raylib_test_end();
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/measure_text", test_measure_text, clay_setup, clay_tear_down, MUNIT_TEST_OPTION_NONE, NULL },
    { "/layout_commands", test_layout_commands, clay_setup, clay_tear_down, MUNIT_TEST_OPTION_NONE, NULL },
    { "/render_frame", test_render_frame, clay_setup, clay_tear_down, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/clay_raylib", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
