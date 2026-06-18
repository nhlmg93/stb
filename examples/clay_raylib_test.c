#define MUNIT_NO_FORK
#define CLAY_IMPLEMENTATION
#define CLAY_RAYLIB_IMPLEMENTATION
#include "../clay.h"
#include "../clay_raylib.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <stdlib.h>
#include <string.h>

static Clay_Arena g_arena;
static Font g_font;
static int g_raylib_ready;

static void on_clay_error(Clay_ErrorData err) {
    (void)err;
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

static MunitResult test_measure_text(const MunitParameter params[], void *user_data) {
    Clay_TextElementConfig cfg = { .fontSize = 32 };
    static const char label[] = "Clay raylib test";
    Clay_StringSlice text = { .length = (int32_t)(sizeof label - 1), .chars = label, .baseChars = label };
    Clay_Dimensions dims;

    (void) params;
    (void) user_data;

    if (!g_raylib_ready) return MUNIT_SKIP;

    dims = Clay_Raylib_MeasureText(text, &cfg, &g_font);
    munit_assert_true(dims.width > 0);
    munit_assert_true(dims.height > 0);
    return MUNIT_OK;
}

static MunitResult test_layout_commands(const MunitParameter params[], void *user_data) {
    Clay_RenderCommandArray cmds = build_ui();

    (void) params;
    (void) user_data;

    munit_assert_true(cmds.length > 0);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_TEXT) >= 1);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_RECTANGLE) >= 1);
    return MUNIT_OK;
}

static MunitResult test_render_frame(const MunitParameter params[], void *user_data) {
    Clay_RenderCommandArray cmds;
    Font fonts[1];

    (void) params;
    (void) user_data;

    if (!g_raylib_ready) return MUNIT_SKIP;

    fonts[0] = g_font;
    BeginDrawing();
    ClearBackground((Color){20, 20, 28, 255});
    cmds = build_ui();
    Clay_Raylib_Render(cmds, fonts);
    EndDrawing();
    munit_assert_true(IsWindowReady());
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/measure_text", test_measure_text, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/layout_commands", test_layout_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/render_frame", test_render_frame, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/clay_raylib", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    Clay_Dimensions dims = {640, 480};
    uint64_t mem_size;
    int rc;

    mem_size = Clay_MinMemorySize();
    g_arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc((size_t)mem_size));
    Clay_Initialize(g_arena, dims, (Clay_ErrorHandler){ .errorHandlerFunction = on_clay_error });

    if (has_display()) {
        Clay_Raylib_Initialize(640, 480, "clay_raylib test", FLAG_WINDOW_HIDDEN);
        g_font = GetFontDefault();
        Clay_SetMeasureTextFunction(Clay_Raylib_MeasureText, &g_font);
        g_raylib_ready = 1;
    }

    rc = munit_suite_main(&suite, NULL, argc, argv);

    if (g_raylib_ready) Clay_Raylib_Close();
    free(g_arena.memory);
    return rc;
}
