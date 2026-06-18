#define _DEFAULT_SOURCE
#define CLAY_IMPLEMENTATION
#define CLAY_TERM_IMPLEMENTATION
#define CLAY_TERM_TERMBOX
#define TB_OPT_ATTR_W 32
#define TB_IMPL
#include "../clay.h"
#include "../clay_term.h"

#include <stdlib.h>

static float g_cell = 9.0f;

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
            CLAY_TEXT(CLAY_STRING("Clay + termbox2"), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = {230, 230, 230, 255} }));
            CLAY_TEXT(CLAY_STRING("Press q to quit"), CLAY_TEXT_CONFIG({ .fontSize = 14, .textColor = {180, 180, 180, 255} }));
            CLAY(CLAY_ID("Inner"),
                 { .layout = { .sizing = { CLAY_SIZING_GROW(0, 0), CLAY_SIZING_FIXED(80) } },
                   .backgroundColor = {60, 66, 82, 255} }) {
                CLAY_TEXT(CLAY_STRING("Layout box"), CLAY_TEXT_CONFIG({ .fontSize = 14, .textColor = {200, 200, 200, 255} }));
            }
        }
    }
    return Clay_EndLayout(0);
}

int main(void) {
    Clay_Dimensions dims;
    uint64_t mem_size;
    Clay_Arena arena;
    struct tb_event ev;

    Clay_Term_Init();
    dims.width = (float)tb_width() * g_cell;
    dims.height = (float)tb_height() * g_cell;

    mem_size = Clay_MinMemorySize();
    arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc((size_t)mem_size));
    Clay_Initialize(arena, dims, (Clay_ErrorHandler){ .errorHandlerFunction = on_clay_error });
    Clay_SetMeasureTextFunction(Clay_Term_MeasureText, &g_cell);

    for (;;) {
        Clay_RenderCommandArray cmds = build_ui();
        Clay_Term_Render(cmds);
        if (Clay_Term_PollEvent(&ev) != TB_OK) continue;
        if (ev.type == TB_EVENT_KEY && ev.key == 'q') break;
    }

    Clay_Term_Shutdown();
    free(arena.memory);
    return 0;
}
