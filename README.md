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

### clay.h

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

### http.h

[http.h](https://github.com/mattiasgustavsson/libs/blob/master/http.h) by Mattias Gustavsson — basic HTTP GET/POST over sockets (no HTTPS).

```c
#define HTTP_IMPLEMENTATION
#include "http.h"

http_t *req = http_get("http://example.com/", NULL);
while (http_process(req) == HTTP_STATUS_PENDING) { }
http_release(req);
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

### tuibox.h

[tuibox](https://github.com/Cubified/tuibox) by Cubified — mouse-driven terminal UI. Boxes, text, keyboard/hover/click events; no ncurses.

```c
#include "tuibox.h"

ui_t u;
ui_new(0, &u);
ui_text(UI_CENTER_X, UI_CENTER_Y, "hello world!", 0, NULL, NULL, &u);
ui_draw(&u);
ui_loop(&u) {
    ui_update(&u);
}
ui_free(&u);
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
| `stb_ds.h` | Public domain, Sean Barrett |
| `http.h` | Public domain, Mattias Gustavsson |
| `minicoro.h` | MIT, Eduardo Bart |
| `tuibox.h` | MIT (includes rxi vec.h) |
