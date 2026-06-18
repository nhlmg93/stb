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

See `examples/clay_term_demo.c`.

[Clay](https://github.com/nicbarker/clay) — flexbox-style 2D UI layout in C. Renderer-agnostic; outputs layout primitives for your draw code.

```c
#define CLAY_IMPLEMENTATION
#include "clay.h"
```

### stb_ds.h

[stb_ds](https://github.com/nothings/stb/blob/master/stb_ds.h) by Sean Barrett — dynamic arrays and hash tables for C.

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

int *items = NULL;
arrpush(items, 42);
arrfree(items);
```

### test.h

Minimal test harness for examples. Single header, explicit test list, no external deps.

```c
#define TEST_IMPLEMENTATION
#include "test.h"

TEST(sanity) {
    ASSERT(1 + 1 == 2);
}

int main(void) {
    const test_case cases[] = { TEST_ENTRY(sanity) };
    return test_run(cases, 1);
}
```

See `examples/test_smoke.c` and `examples/http_test.c`.

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

```makefile
# examples/Makefile
CFLAGS += -Ipath/to/stb
LDFLAGS += -lssl -lcrypto
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
    printf("hello\n");
    mco_yield(co);
    printf("again\n");
}

mco_coro *co;
mco_create(&co, mco_desc_init(task, 0));
mco_resume(co);
mco_resume(co);
mco_destroy(co);
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
| `stb_ds.h` | Public domain, Sean Barrett |
| `test.h` | MIT |
| `http.h` | MIT; HTTP poll model derived from http.h (Mattias Gustavsson) |
| `minicoro.h` | MIT, Eduardo Bart |
