/*
 * clay_raylib.h — stb
 *
 * Clay renderer for raylib. Requires clay.h and system raylib (+ raymath).
 *
 *   #define CLAY_IMPLEMENTATION
 *   #define CLAY_RAYLIB_IMPLEMENTATION
 *   #include "raylib.h"
 *   #include "raymath.h"
 *   #include "clay.h"
 *   #include "clay_raylib.h"
 *
 * Link raylib (e.g. -lraylib, or pkg-config --libs raylib).
 */

#ifndef CLAY_RAYLIB_H
#define CLAY_RAYLIB_H

#include "clay.h"
#include "raylib.h"

Clay_Dimensions Clay_Raylib_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_Raylib_Initialize(int width, int height, const char *title, unsigned int flags);
void Clay_Raylib_Close(void);
void Clay_Raylib_Render(Clay_RenderCommandArray renderCommands, Font *fonts);

/* Used by the built-in 3D model custom element renderer. */
extern Camera Clay_Raylib_camera;

typedef enum {
    CLAY_RAYLIB_CUSTOM_3D_MODEL
} Clay_Raylib_CustomType;

typedef struct {
    Model model;
    float scale;
    Vector3 position;
    Matrix rotation;
} Clay_Raylib_CustomModel;

typedef struct {
    Clay_Raylib_CustomType type;
    union {
        Clay_Raylib_CustomModel model;
    } customData;
} Clay_Raylib_CustomElement;

#endif /* CLAY_RAYLIB_H */

#ifdef CLAY_RAYLIB_IMPLEMENTATION

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raymath.h"

#define CLAY_RAYLIB_RECT(r)                                                                                \
    (Rectangle) { .x = (r).x, .y = (r).y, .width = (r).width, .height = (r).height }
#define CLAY_RAYLIB_COLOR(c)                                                                               \
    (Color) {                                                                                              \
        .r = (unsigned char)roundf((c).r), .g = (unsigned char)roundf((c).g), .b = (unsigned char)roundf((c).b), \
        .a = (unsigned char)roundf((c).a)                                                                  \
    }

Camera Clay_Raylib_camera;

static Shader clay_raylib_overlay_shader;
static int clay_raylib_overlay_color_loc;
static bool clay_raylib_overlay_enabled;

static const char *clay_raylib_overlay_shader_code =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 overlayColor;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord) * fragColor;\n"
    "    vec3 blendedRGB = mix(texelColor.rgb, overlayColor.rgb, overlayColor.a);\n"
    "    finalColor = vec4(blendedRGB, texelColor.a);\n"
    "}";

static char *clay_raylib_text_buf;
static int clay_raylib_text_buf_len;

static void clay_raylib_init_overlay(void) {
    clay_raylib_overlay_shader = LoadShaderFromMemory(0, clay_raylib_overlay_shader_code);
    clay_raylib_overlay_color_loc = GetShaderLocation(clay_raylib_overlay_shader, "overlayColor");
}

static void clay_raylib_set_overlay(Color color) {
    float color_float[4] = {(float)color.r / 255.0f, (float)color.g / 255.0f, (float)color.b / 255.0f,
                            (float)color.a / 255.0f};
    clay_raylib_overlay_enabled = true;
    SetShaderValue(clay_raylib_overlay_shader, clay_raylib_overlay_color_loc, color_float, SHADER_UNIFORM_VEC4);
    BeginShaderMode(clay_raylib_overlay_shader);
}

static void clay_raylib_disable_overlay(void) {
    if (clay_raylib_overlay_enabled) {
        EndShaderMode();
        clay_raylib_overlay_enabled = false;
    }
}

static Ray clay_raylib_screen_to_world(Vector2 position, Camera camera, int screen_width, int screen_height,
                                       float z_distance) {
    Ray ray = {0};
    float x = (2.0f * position.x) / (float)screen_width - 1.0f;
    float y = 1.0f - (2.0f * position.y) / (float)screen_height;
    Vector3 device_coords = {x, y, 1.0f};
    Matrix mat_view = MatrixLookAt(camera.position, camera.target, camera.up);
    Matrix mat_proj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE) {
        mat_proj = MatrixPerspective(camera.fovy * DEG2RAD, ((double)screen_width / (double)screen_height), 0.01f,
                                     z_distance);
    } else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double aspect = (double)screen_width / (double)screen_height;
        double top = camera.fovy / 2.0;
        double right = top * aspect;
        mat_proj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    {
        Vector3 near_point =
            Vector3Unproject((Vector3){device_coords.x, device_coords.y, 0.0f}, mat_proj, mat_view);
        Vector3 far_point =
            Vector3Unproject((Vector3){device_coords.x, device_coords.y, 1.0f}, mat_proj, mat_view);
        Vector3 direction = Vector3Normalize(Vector3Subtract(far_point, near_point));
        ray.position = far_point;
        ray.direction = direction;
    }
    return ray;
}

Clay_Dimensions Clay_Raylib_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Clay_Dimensions text_size = {0};
    float max_text_width = 0.0f;
    float line_text_width = 0.0f;
    int line_char_count = 0;
    Font *fonts = (Font *)userData;
    Font font = fonts[config->fontId];
    float scale_factor;
    int i;

    if (!font.glyphs) font = GetFontDefault();
    scale_factor = config->fontSize / (float)font.baseSize;

    for (i = 0; i < text.length; ++i, line_char_count++) {
        if (text.chars[i] == '\n') {
            if (line_text_width > max_text_width) max_text_width = line_text_width;
            line_text_width = 0;
            line_char_count = 0;
            continue;
        }
        {
            int index = text.chars[i] - 32;
            if (font.glyphs[index].advanceX != 0) line_text_width += font.glyphs[index].advanceX;
            else line_text_width += (font.recs[index].width + font.glyphs[index].offsetX);
        }
    }

    if (line_text_width > max_text_width) max_text_width = line_text_width;
    text_size.width = max_text_width * scale_factor + (line_char_count * config->letterSpacing);
    text_size.height = config->fontSize;
    return text_size;
}

void Clay_Raylib_Initialize(int width, int height, const char *title, unsigned int flags) {
    SetConfigFlags(flags);
    InitWindow(width, height, title);
    clay_raylib_init_overlay();
}

void Clay_Raylib_Close(void) {
    if (clay_raylib_text_buf) {
        free(clay_raylib_text_buf);
        clay_raylib_text_buf = NULL;
        clay_raylib_text_buf_len = 0;
    }
    CloseWindow();
}

void Clay_Raylib_Render(Clay_RenderCommandArray renderCommands, Font *fonts) {
    int j;
    for (j = 0; j < renderCommands.length; j++) {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, j);
        Clay_BoundingBox boundingBox = renderCommand->boundingBox;

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *textData = &renderCommand->renderData.text;
            Font fontToUse = fonts[textData->fontId];
            int text_len = textData->stringContents.length + 1;

            if (text_len > clay_raylib_text_buf_len) {
                free(clay_raylib_text_buf);
                clay_raylib_text_buf = (char *)malloc((size_t)text_len);
                clay_raylib_text_buf_len = text_len;
            }
            if (!clay_raylib_text_buf) break;

            memcpy(clay_raylib_text_buf, textData->stringContents.chars, (size_t)textData->stringContents.length);
            clay_raylib_text_buf[textData->stringContents.length] = '\0';
            DrawTextEx(fontToUse, clay_raylib_text_buf, (Vector2){boundingBox.x, boundingBox.y},
                       (float)textData->fontSize, (float)textData->letterSpacing,
                       CLAY_RAYLIB_COLOR(textData->textColor));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Texture2D imageTexture = *(Texture2D *)renderCommand->renderData.image.imageData;
            Clay_Color tintColor = renderCommand->renderData.image.backgroundColor;
            if (tintColor.r == 0 && tintColor.g == 0 && tintColor.b == 0 && tintColor.a == 0) {
                tintColor = (Clay_Color){255, 255, 255, 255};
            }
            DrawTexturePro(imageTexture, (Rectangle){0, 0, (float)imageTexture.width, (float)imageTexture.height},
                           CLAY_RAYLIB_RECT(boundingBox), (Vector2){0, 0}, 0, CLAY_RAYLIB_COLOR(tintColor));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            BeginScissorMode((int)roundf(boundingBox.x), (int)roundf(boundingBox.y), (int)roundf(boundingBox.width),
                             (int)roundf(boundingBox.height));
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            EndScissorMode();
            break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
            clay_raylib_set_overlay(CLAY_RAYLIB_COLOR(renderCommand->renderData.overlayColor.color));
            break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            clay_raylib_disable_overlay();
            break;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
            if (config->cornerRadius.topLeft > 0) {
                float radius =
                    (config->cornerRadius.topLeft * 2) /
                    (float)((boundingBox.width > boundingBox.height) ? boundingBox.height : boundingBox.width);
                DrawRectangleRounded(CLAY_RAYLIB_RECT(boundingBox), radius, 8,
                                     CLAY_RAYLIB_COLOR(config->backgroundColor));
            } else {
                DrawRectangle((int)boundingBox.x, (int)boundingBox.y, (int)boundingBox.width, (int)boundingBox.height,
                              CLAY_RAYLIB_COLOR(config->backgroundColor));
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *config = &renderCommand->renderData.border;
            if (config->width.left > 0) {
                DrawRectangleV((Vector2){boundingBox.x, boundingBox.y + config->cornerRadius.topLeft},
                               (Vector2){config->width.left,
                                         boundingBox.height - config->cornerRadius.topLeft -
                                             config->cornerRadius.bottomLeft},
                               CLAY_RAYLIB_COLOR(config->color));
            }
            if (config->width.right > 0) {
                DrawRectangleV(
                    (Vector2){boundingBox.x + boundingBox.width - config->width.right,
                              boundingBox.y + config->cornerRadius.topRight},
                    (Vector2){config->width.right,
                              boundingBox.height - config->cornerRadius.topRight - config->cornerRadius.bottomRight},
                    CLAY_RAYLIB_COLOR(config->color));
            }
            if (config->width.top > 0) {
                DrawRectangleV((Vector2){boundingBox.x + config->cornerRadius.topLeft, boundingBox.y},
                             (Vector2){boundingBox.width - config->cornerRadius.topLeft - config->cornerRadius.topRight,
                                       config->width.top},
                             CLAY_RAYLIB_COLOR(config->color));
            }
            if (config->width.bottom > 0) {
                DrawRectangleV(
                    (Vector2){boundingBox.x + config->cornerRadius.bottomLeft,
                              boundingBox.y + boundingBox.height - config->width.bottom},
                    (Vector2){boundingBox.width - config->cornerRadius.bottomLeft - config->cornerRadius.bottomRight,
                              config->width.bottom},
                    CLAY_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.topLeft > 0) {
                DrawRing((Vector2){roundf(boundingBox.x + config->cornerRadius.topLeft),
                                   roundf(boundingBox.y + config->cornerRadius.topLeft)},
                         roundf(config->cornerRadius.topLeft - config->width.top), config->cornerRadius.topLeft, 180,
                         270, 10, CLAY_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.topRight > 0) {
                DrawRing((Vector2){roundf(boundingBox.x + boundingBox.width - config->cornerRadius.topRight),
                                   roundf(boundingBox.y + config->cornerRadius.topRight)},
                         roundf(config->cornerRadius.topRight - config->width.top), config->cornerRadius.topRight, 270,
                         360, 10, CLAY_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.bottomLeft > 0) {
                DrawRing((Vector2){roundf(boundingBox.x + config->cornerRadius.bottomLeft),
                                   roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomLeft)},
                         roundf(config->cornerRadius.bottomLeft - config->width.bottom),
                         config->cornerRadius.bottomLeft, 90, 180, 10, CLAY_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.bottomRight > 0) {
                DrawRing((Vector2){roundf(boundingBox.x + boundingBox.width - config->cornerRadius.bottomRight),
                                   roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomRight)},
                         roundf(config->cornerRadius.bottomRight - config->width.bottom),
                         config->cornerRadius.bottomRight, 0.1f, 90, 10, CLAY_RAYLIB_COLOR(config->color));
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            Clay_CustomRenderData *config = &renderCommand->renderData.custom;
            Clay_Raylib_CustomElement *customElement = (Clay_Raylib_CustomElement *)config->customData;
            if (!customElement) break;
            if (customElement->type == CLAY_RAYLIB_CUSTOM_3D_MODEL) {
                Clay_BoundingBox rootBox = renderCommands.internalArray[0].boundingBox;
                float scaleValue = CLAY__MIN(CLAY__MIN(1, 768 / rootBox.height) * CLAY__MAX(1, rootBox.width / 1024),
                                             1.5f);
                Ray positionRay = clay_raylib_screen_to_world(
                    (Vector2){renderCommand->boundingBox.x + renderCommand->boundingBox.width / 2,
                              renderCommand->boundingBox.y + (renderCommand->boundingBox.height / 2) + 20},
                    Clay_Raylib_camera, (int)roundf(rootBox.width), (int)roundf(rootBox.height), 140);
                BeginMode3D(Clay_Raylib_camera);
                DrawModel(customElement->customData.model.model, positionRay.position,
                          customElement->customData.model.scale * scaleValue, WHITE);
                EndMode3D();
            }
            break;
        }
        default:
            fprintf(stderr, "clay_raylib: unhandled render command %d\n", (int)renderCommand->commandType);
            break;
        }
    }
}

#endif /* CLAY_RAYLIB_IMPLEMENTATION */

/*
 * clay_raylib.h — zlib/libpng license (renderer derived from Clay by Nic Barker)
 */
