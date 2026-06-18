#define _DEFAULT_SOURCE
#define CLAY_IMPLEMENTATION
#define CLAY_TERM_IMPLEMENTATION
#include "../clay.h"
#include "../clay_term.h"

#define MUNIT_IMPLEMENTATION
#include "../munit.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Clay_Arena g_arena;
static int g_col = 8;

static void on_clay_error(Clay_ErrorData err) {
    (void)err;
}

static Clay_RenderCommandArray build_ui(void) {
    Clay_BeginLayout();
    CLAY(CLAY_ID("Root"), { .layout = { .sizing = { CLAY_SIZING_GROW(0, 0), CLAY_SIZING_GROW(0, 0) } } }) {
        CLAY(CLAY_ID("Panel"),
             { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM,
                           .padding = CLAY_PADDING_ALL(8),
                           .childGap = 8,
                           .sizing = { CLAY_SIZING_GROW(0, 0), CLAY_SIZING_GROW(0, 0) } },
               .backgroundColor = {40, 44, 52, 255},
               .border = { .width = {2, 2, 2, 2, 0}, .color = {120, 130, 150, 255} } }) {
            CLAY_TEXT(CLAY_STRING("Clay terminal test"), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = {230, 230, 230, 255} }));
            CLAY(CLAY_ID("Inner"),
                 { .layout = { .sizing = { CLAY_SIZING_GROW(0, 0), CLAY_SIZING_FIXED(80) } },
                   .backgroundColor = {60, 66, 82, 255} }) {
                CLAY_TEXT(CLAY_STRING("Layout box"), CLAY_TEXT_CONFIG({ .fontSize = 14, .textColor = {200, 200, 200, 255} }));
            }
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

static int capture_stdout(char *buf, size_t cap, void (*fn)(void)) {
    int pipefd[2];
    int saved_stdout;
    ssize_t n;

    if (pipe(pipefd) != 0) return -1;
    saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    fn();
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    n = read(pipefd[0], buf, cap - 1);
    close(pipefd[0]);
    if (n < 0) return -1;
    buf[n] = '\0';
    return 0;
}

static void render_ui(void) {
    Clay_RenderCommandArray cmds = build_ui();
    Clay_Term_Render(cmds, 80, 24, g_col);
}

static MunitResult test_measure_text(const MunitParameter params[], void *user_data) {
    Clay_TextElementConfig cfg = { .fontSize = 16 };
    static const char hello[] = "hello";
    Clay_StringSlice text = { .length = 5, .chars = hello, .baseChars = hello };
    Clay_Dimensions dims;

    (void) params;
    (void) user_data;

    dims = Clay_Term_MeasureText(text, &cfg, &g_col);
    munit_assert_true(dims.width > 0);
    munit_assert_true(dims.height > 0);
    return MUNIT_OK;
}

static MunitResult test_layout_commands(const MunitParameter params[], void *user_data) {
    Clay_RenderCommandArray cmds = build_ui();

    (void) params;
    (void) user_data;

    munit_assert_true(cmds.length > 0);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_TEXT) >= 2);
    munit_assert_true(count_commands(cmds, CLAY_RENDER_COMMAND_TYPE_RECTANGLE) >= 1);
    return MUNIT_OK;
}

static MunitResult test_ansi_render(const MunitParameter params[], void *user_data) {
    char out[4096];

    (void) params;
    (void) user_data;

    munit_assert_int(capture_stdout(out, sizeof out, render_ui), ==, 0);
    munit_assert_not_null(strstr(out, "\033["));
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/measure_text", test_measure_text, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/layout_commands", test_layout_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/ansi_render", test_ansi_render, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { (char *)"/clay_term", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    Clay_Dimensions dims = {720, 480};
    uint64_t mem_size;
    int rc;

    mem_size = Clay_MinMemorySize();
    g_arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc((size_t)mem_size));
    Clay_Initialize(g_arena, dims, (Clay_ErrorHandler){ .errorHandlerFunction = on_clay_error });
    Clay_SetMeasureTextFunction(Clay_Term_MeasureText, &g_col);

    rc = munit_suite_main(&suite, NULL, argc, argv);
    free(g_arena.memory);
    return rc;
}
