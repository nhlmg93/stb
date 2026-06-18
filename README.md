# stb

Single-header, stb-style C libraries — drop in a `.h`, define `*_IMPLEMENTATION` in one `.c` file, done.

## Libraries

### json.h

Based on [json.h by Neil Henning](https://github.com/sheredom/json.h) with stb-style `JSON_IMPLEMENTATION` guards.

```c
#define JSON_IMPLEMENTATION
#include "json.h"

struct json_value_s *root = json_parse(text, size);
free(root);
```

See `examples/json_test.c`.

### clay.h

[Clay](https://github.com/nicbarker/clay) — flexbox-style 2D UI layout in C. Renderer-agnostic; outputs layout primitives for your draw code.

```c
#define CLAY_IMPLEMENTATION
#include "clay.h"
```

See `examples/clay_test.c`.

### clay.h + termbox2.h + clay_term.h

[Clay](https://github.com/nicbarker/clay) handles layout; [termbox2](https://github.com/termbox/termbox2) is the terminal I/O layer Clay's official TUI backend uses; **clay_term.h** renders `Clay_RenderCommandArray` to the terminal.

```c
#define CLAY_IMPLEMENTATION
#define CLAY_TERM_IMPLEMENTATION
#define CLAY_TERM_TERMBOX
#define TB_OPT_ATTR_W 32
#define TB_IMPL
#include "termbox2.h"
#include "clay.h"
#include "clay_term.h"

Clay_Term_Init();
Clay_SetMeasureTextFunction(Clay_Term_MeasureText, &cellWidth);
Clay_RenderCommandArray cmds = /* Clay_EndLayout(0) after building UI */;
Clay_Term_Render(cmds);
Clay_Term_Shutdown();
```

ANSI-only backend (no termbox2): `#define CLAY_TERM_IMPLEMENTATION` without `CLAY_TERM_TERMBOX`, then `Clay_Term_Render(cmds, cols, rows, columnWidth)`.

See `examples/clay_term_test.c`.

### clay.h + clay_raylib.h

[Clay](https://github.com/nicbarker/clay) layout with a [raylib](https://www.raylib.com/) renderer (ported from Clay's official raylib backend). Requires system raylib — link `-lraylib` (or `pkg-config --libs raylib`).

```c
#define CLAY_IMPLEMENTATION
#define CLAY_RAYLIB_IMPLEMENTATION
#include "raylib.h"
#include "raymath.h"
#include "clay.h"
#include "clay_raylib.h"

Clay_Raylib_Initialize(800, 450, "app", 0);
Clay_SetMeasureTextFunction(Clay_Raylib_MeasureText, fonts);
Clay_Raylib_Render(Clay_EndLayout(0), fonts);
Clay_Raylib_Close();
```

See `examples/clay_raylib_test.c`.

### termbox2.h

[termbox2](https://github.com/termbox/termbox2) terminal I/O (vendored). Used by `clay_term.h` when `CLAY_TERM_TERMBOX` is defined.

```c
#define TB_OPT_ATTR_W 32
#define TB_IMPL
#include "termbox2.h"

tb_init();
tb_set_cell(0, 0, 'A', TB_WHITE, TB_BLACK);
tb_present();
tb_shutdown();
```

See `examples/termbox_test.c` (skips when stdout is not a TTY).

### arena.h

Bump-pointer arena allocator — fast linear allocations with O(1) reset. Ported from the arena in the game-1 project.

```c
#define ARENA_IMPLEMENTATION
#include "arena.h"

Arena arena = arena_create(4096);
int *n = arena_alloc(&arena, sizeof *n);
char *s = arena_strdup(&arena, "hello");
arena_reset(&arena);
arena_destroy(&arena);
```

See `examples/arena_test.c`.

### stb_ds.h

[stb_ds](https://github.com/nothings/stb/blob/master/stb_ds.h) by Sean Barrett — dynamic arrays and hash tables for C.

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

int *items = NULL;
arrpush(items, 42);
arrfree(items);
```

See `examples/stb_ds_test.c`.

### munit.h

[µnit](https://github.com/nemequ/munit) by Evan Nemerson — vendored STB-style unit test framework.

```c
#define MUNIT_IMPLEMENTATION
#include "munit.h"

static MunitResult test_example(const MunitParameter params[], void *data) {
    (void) params; (void) data;
    munit_assert_int(1 + 1, ==, 2);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    { "/example", test_example, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite suite = { "/", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE };

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
```

See `examples/test_smoke.c`.

### http.h

HTTP/HTTPS client with streaming and SSE. Poll-based API extending [http.h](https://github.com/mattiasgustavsson/libs/blob/master/http.h) by Mattias Gustavsson (ported here, not a dependency).

HTTPS uses system OpenSSL (`libssl`/`libcrypto`) — no bundled TLS library. Link `-lssl -lcrypto` (or `pkg-config --libs openssl`).

```c
#define HTTP_IMPLEMENTATION
#include "http.h"

http_t *h = http_get("https://example.com/", NULL);
while (http_process(h) == HTTP_STATUS_PENDING) { }
http_release(h);
```

See `examples/http_test.c` — local test server covers GET/POST, headers, chunks, and SSE. Set `HTTP_TEST_NETWORK=1` to also run external HTTPS checks.

```makefile
# examples/
make test
```

Arena alloc — pass your arena pointer as `memctx`, override alloc before include:

```c
#define HTTP_MALLOC(ctx, n) my_arena_alloc(ctx, n)
#define HTTP_FREE(ctx, p)   ((void)0)
#define HTTP_IMPLEMENTATION
#include "http.h"
```

HTTP-only (no OpenSSL): `#define HTTP_HTTPS 0` before include.

SSE streaming:

```c
http_t *h = http_post(url, body, body_len, NULL);
http_header(h, "Content-Type", "application/json");
http_sse(h, 1);
http_on_sse(h, on_sse, NULL);
while (http_process(h) == HTTP_STATUS_PENDING) { }
```

### minicoro.h

[minicoro](https://github.com/edubart/minicoro) by Eduardo Bart — stackful green threads (asymmetric coroutines). Lua-style yield/resume; not a parallel scheduler.

```c
#define MINICORO_IMPL
#include "minicoro.h"

static void task(mco_coro *co) {
    mco_yield(co);
}

mco_coro *co;
mco_create(&co, mco_desc_init(task, 0));
mco_resume(co);
mco_resume(co);
mco_destroy(co);
```

See `examples/minicoro_test.c`.

## Tests

Every library has a matching test in `examples/`:

| Library | Test |
|---------|------|
| `munit.h` | `test_smoke.c` |
| `json.h` | `json_test.c` |
| `arena.h` | `arena_test.c` |
| `stb_ds.h` | `stb_ds_test.c` |
| `minicoro.h` | `minicoro_test.c` |
| `clay.h` | `clay_test.c` |
| `termbox2.h` | `termbox_test.c` |
| `http.h` | `http_test.c` |
| `clay_term.h` | `clay_term_test.c` |
| `clay_raylib.h` | `clay_raylib_test.c` |

```makefile
cd examples && make test
```

## Usage

Add this repo to your include path and include headers directly:

```makefile
CFLAGS += -Ivendor/stb
```

Or fetch headers at build time:

```makefile
STB = https://raw.githubusercontent.com/nhlmg93/stb/master

vendor/stb/json.h:
	mkdir -p vendor/stb
	curl -fsSL $(STB)/json.h -o $@
```

## License

| File | License |
|------|---------|
| `json.h` | Public domain (Unlicense), from upstream |
| `clay.h` | zlib, Copyright (c) Nic Barker |
| `termbox2.h` | MIT, Adam Saponara / nsf |
| `clay_term.h` | MIT; terminal renderers derived from Clay |
| `clay_raylib.h` | zlib/libpng; raylib renderer derived from Clay |
| `arena.h` | Public domain / MIT |
| `stb_ds.h` | Public domain, Sean Barrett |
| `munit.h` | MIT, Evan Nemerson (vendored from µnit) |
| `http.h` | MIT; HTTP poll model derived from http.h (Mattias Gustavsson) |
| `minicoro.h` | MIT, Eduardo Bart |
