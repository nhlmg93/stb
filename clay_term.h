/*
 * clay_term.h — stb
 *
 * Clay terminal renderers. Requires clay.h (declare CLAY_IMPLEMENTATION in one TU).
 *
 * Backends (pick one before CLAY_TERM_IMPLEMENTATION):
 *
 *   CLAY_TERM_ANSI     — printf/ANSI only, no extra deps (default)
 *   CLAY_TERM_TERMBOX  — truecolor via termbox2.h (#define TB_IMPL + TB_OPT_ATTR_W 32)
 *
 *   #define CLAY_IMPLEMENTATION
 *   #define CLAY_TERM_IMPLEMENTATION
 *   #define CLAY_TERM_TERMBOX
 *   #define TB_OPT_ATTR_W 32
 *   #define TB_IMPL
 *   #include "clay.h"
 *   #include "clay_term.h"
 */

#ifndef CLAY_TERM_H
#define CLAY_TERM_H

#include "clay.h"

Clay_Dimensions Clay_Term_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

#if defined(CLAY_TERM_TERMBOX)
struct tb_event;
void Clay_Term_Init(void);
void Clay_Term_Shutdown(void);
void Clay_Term_Render(Clay_RenderCommandArray commands);
int Clay_Term_PollEvent(struct tb_event *ev);
#else
void Clay_Term_Render(Clay_RenderCommandArray commands, int width, int height, int columnWidth);
#endif

#endif /* CLAY_TERM_H */

#ifdef CLAY_TERM_IMPLEMENTATION

#ifndef CLAY_TERM_TERMBOX
#define CLAY_TERM_ANSI 1
#endif

#include <stdio.h>
#include <string.h>

#if defined(CLAY_TERM_TERMBOX)
#ifndef TB_OPT_ATTR_W
#define TB_OPT_ATTR_W 32
#endif
#include "termbox2.h"
#endif

Clay_Dimensions Clay_Term_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Clay_Dimensions size = {0};
    float cell_w;
    float cell_h;
    float line_w = 0;
    float max_w = 0;
    int i;

    (void)config;

#if defined(CLAY_TERM_TERMBOX)
    cell_w = *(float *)userData;
    cell_h = cell_w;
#else
    cell_w = (float)*(int *)userData;
    cell_h = cell_w;
#endif

    for (i = 0; i < text.length; ++i) {
        if (text.chars[i] == '\n') {
            if (line_w > max_w) max_w = line_w;
            line_w = 0;
            size.height += cell_h;
            continue;
        }
        line_w += cell_w;
    }
    if (line_w > max_w) max_w = line_w;
    if (size.height == 0) size.height = cell_h;
    size.width = max_w;
    return size;
}

#if defined(CLAY_TERM_ANSI)

static int clay_term_point_in_rect(Clay_Vector2 point, Clay_BoundingBox rect) {
    return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y &&
           point.y < rect.y + rect.height;
}

static void clay_term_ansi_move(int x, int y) {
    printf("\033[%d;%dH", y + 1, x + 1);
}

static void clay_term_ansi_rect(int x0, int y0, int w, int h, Clay_Color color, Clay_BoundingBox clip) {
    float avg = (color.r + color.g + color.b + color.a) / 4.0f / 255.0f;
    const char *ch = avg > 0.75f ? "█" : avg > 0.5f ? "▓" : avg > 0.25f ? "▒" : "░";
    int y, x;

    for (y = y0; y < y0 + h; y++) {
        for (x = x0; x < x0 + w; x++) {
            Clay_Vector2 p = {.x = (float)x, .y = (float)y};
            if (!clay_term_point_in_rect(p, clip)) continue;
            clay_term_ansi_move(x, y);
            fputs(ch, stdout);
        }
    }
}

void Clay_Term_Render(Clay_RenderCommandArray renderCommands, int width, int height, int columnWidth) {
    Clay_BoundingBox clip = {0, 0, (float)width, (float)height};
    int j;

    printf("\033[H\033[J");

    for (j = 0; j < renderCommands.length; j++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&renderCommands, j);
        Clay_BoundingBox box = {
            .x = (float)((int)(cmd->boundingBox.x / columnWidth + 0.5f)),
            .y = (float)((int)(cmd->boundingBox.y / columnWidth + 0.5f)),
            .width = (float)((int)(cmd->boundingBox.width / columnWidth + 0.5f)),
            .height = (float)((int)(cmd->boundingBox.height / columnWidth + 0.5f)),
        };

        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData data = cmd->renderData.text;
            Clay_StringSlice text = data.stringContents;
            int line = 0;
            int col = 0;
            int i;
            for (i = 0; i < text.length; i++) {
                int cx, cy;
                if (text.chars[i] == '\n') {
                    line++;
                    col = 0;
                    continue;
                }
                cx = (int)box.x + col;
                cy = (int)box.y + line;
                if (cy > clip.y + clip.height) break;
                if (clay_term_point_in_rect((Clay_Vector2){.x = (float)cx, .y = (float)cy}, clip)) {
                    clay_term_ansi_move(cx, cy);
                    fputc(text.chars[i], stdout);
                }
                col++;
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            clip = box;
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            clip = (Clay_BoundingBox){0, 0, (float)width, (float)height};
            break;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            clay_term_ansi_rect((int)box.x, (int)box.y, (int)box.width, (int)box.height,
                                cmd->renderData.rectangle.backgroundColor, clip);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData b = cmd->renderData.border;
            if (b.width.left > 0) {
                clay_term_ansi_rect((int)box.x, (int)(box.y + b.cornerRadius.topLeft), (int)b.width.left,
                                    (int)(box.height - b.cornerRadius.topLeft - b.cornerRadius.bottomLeft), b.color,
                                    clip);
            }
            if (b.width.right > 0) {
                clay_term_ansi_rect((int)(box.x + box.width - b.width.right), (int)(box.y + b.cornerRadius.topRight),
                                    (int)b.width.right,
                                    (int)(box.height - b.cornerRadius.topRight - b.cornerRadius.bottomRight), b.color,
                                    clip);
            }
            if (b.width.top > 0) {
                clay_term_ansi_rect((int)(box.x + b.cornerRadius.topLeft), (int)box.y,
                                    (int)(box.width - b.cornerRadius.topLeft - b.cornerRadius.topRight), (int)b.width.top,
                                    b.color, clip);
            }
            if (b.width.bottom > 0) {
                clay_term_ansi_rect((int)(box.x + b.cornerRadius.bottomLeft), (int)(box.y + box.height - b.width.bottom),
                                    (int)(box.width - b.cornerRadius.bottomLeft - b.cornerRadius.bottomRight),
                                    (int)b.width.bottom, b.color, clip);
            }
            break;
        }
        default:
            break;
        }
    }

    clay_term_ansi_move(0, height);
}

#else /* CLAY_TERM_TERMBOX */

typedef struct {
    int x, y, w, h;
} clay_term_cell_box;

static float clay_term_cell_w = 9.0f;
static float clay_term_cell_h = 21.0f;
static int clay_term_ready;
static int clay_term_scissor_on;
static clay_term_cell_box clay_term_scissor;

static int clay_term_roundf(float f) {
    int i = (int)f;
    return (f - i > 0.5f) ? i + 1 : i;
}

static clay_term_cell_box clay_term_snap(Clay_BoundingBox box) {
    return (clay_term_cell_box){
        clay_term_roundf(box.x / clay_term_cell_w),
        clay_term_roundf(box.y / clay_term_cell_h),
        clay_term_roundf((box.x + box.width) / clay_term_cell_w) - clay_term_roundf(box.x / clay_term_cell_w),
        clay_term_roundf((box.y + box.height) / clay_term_cell_h) - clay_term_roundf(box.y / clay_term_cell_h),
    };
}

static uintattr_t clay_term_color(Clay_Color c) {
    uintattr_t color = ((uintattr_t)c.r << 16) | ((uintattr_t)c.g << 8) | (uintattr_t)c.b;
    return color ? color : TB_HI_BLACK;
}

static int clay_term_in_scissor(int x, int y) {
    if (!clay_term_scissor_on) return 1;
    return x >= clay_term_scissor.x && x < clay_term_scissor.x + clay_term_scissor.w && y >= clay_term_scissor.y &&
           y < clay_term_scissor.y + clay_term_scissor.h;
}

static void clay_term_cell(int x, int y, uint32_t ch, uintattr_t fg, uintattr_t bg) {
    if (x < 0 || y < 0 || x >= tb_width() || y >= tb_height()) return;
    if (!clay_term_in_scissor(x, y)) return;
    tb_set_cell(x, y, ch, fg, bg);
}

void Clay_Term_Init(void) {
    tb_init();
    tb_set_output_mode(TB_OUTPUT_TRUECOLOR);
    clay_term_ready = 1;
}

void Clay_Term_Shutdown(void) {
    if (clay_term_ready) tb_shutdown();
    clay_term_ready = 0;
}

int Clay_Term_PollEvent(struct tb_event *ev) {
    return tb_poll_event(ev);
}

void Clay_Term_Render(Clay_RenderCommandArray commands) {
    int i;

    if (!clay_term_ready) Clay_Term_Init();
    tb_clear();

    for (i = 0; i < commands.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        clay_term_cell_box box = clay_term_snap(cmd->boundingBox);
        int x0 = box.x < 0 ? 0 : box.x;
        int y0 = box.y < 0 ? 0 : box.y;
        int x1 = box.x + box.w;
        int y1 = box.y + box.h;
        if (x1 > tb_width()) x1 = tb_width();
        if (y1 > tb_height()) y1 = tb_height();

        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            uintattr_t bg = clay_term_color(cmd->renderData.rectangle.backgroundColor);
            int y, x;
            for (y = y0; y < y1; y++)
                for (x = x0; x < x1; x++) clay_term_cell(x, y, ' ', TB_DEFAULT, bg);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData b = cmd->renderData.border;
            uintattr_t bg = clay_term_color(b.color);
            int y, x;
            int inner_x0 = x0 + (b.width.left > 0 ? 1 : 0);
            int inner_x1 = x1 - (b.width.right > 0 ? 1 : 0);
            int inner_y0 = y0 + (b.width.top > 0 ? 1 : 0);
            int inner_y1 = y1 - (b.width.bottom > 0 ? 1 : 0);
            for (y = y0; y < y1; y++) {
                for (x = x0; x < x1; x++) {
                    uint32_t ch = 0;
                    if (inner_x0 <= x && x < inner_x1 && inner_y0 <= y && y < inner_y1) {
                        x = inner_x1 - 1;
                        continue;
                    }
                    if (x < inner_x0 && y < inner_y0) ch = U'\u250c';
                    else if (x >= inner_x1 && y < inner_y0) ch = U'\u2510';
                    else if (x < inner_x0 && y >= inner_y1) ch = U'\u2514';
                    else if (x >= inner_x1 && y >= inner_y1) ch = U'\u2518';
                    else if (x < inner_x0 || x >= inner_x1) ch = U'\u2502';
                    else if (y < inner_y0 || y >= inner_y1) ch = U'\u2500';
                    if (ch) clay_term_cell(x, y, ch, TB_DEFAULT, bg);
                }
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData data = cmd->renderData.text;
            uintattr_t fg = clay_term_color(data.textColor);
            int col = 0;
            int row = 0;
            int ti;
            for (ti = 0; ti < data.stringContents.length; ti++) {
                uint32_t ch;
                int cp_len = tb_utf8_char_to_unicode(&ch, data.stringContents.chars + ti);
                int w, x, y;
                if (cp_len < 0) break;
                ti += cp_len - 1;
                if (ch == '\n') {
                    row++;
                    col = 0;
                    continue;
                }
                x = x0 + col;
                y = y0 + row;
                if (y >= y1) break;
                clay_term_cell(x, y, ch, fg, TB_DEFAULT);
                w = tb_wcwidth(ch);
                if (w < 1) w = 1;
                col += w;
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            clay_term_scissor = (clay_term_cell_box){x0, y0, x1 - x0, y1 - y0};
            clay_term_scissor_on = 1;
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            clay_term_scissor_on = 0;
            break;
        default:
            break;
        }
    }

    tb_present();
}

#endif /* CLAY_TERM_TERMBOX */

#endif /* CLAY_TERM_IMPLEMENTATION */

/*
 * clay_term.h — MIT / zlib portions from Clay terminal renderers (Nic Barker / upstream contributors)
 */
