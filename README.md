# stb_me

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

Clay_BeginLayout(layoutDimensions);
CLAY({ .id = CLAY_ID("Root"), .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } }) {
    CLAY_TEXT(CLAY_STRING("Hello"), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));
}
Clay_EndLayout();
```

## Usage

Add this repo to your include path and include headers directly:

```makefile
CFLAGS += -Ivendor/stb_me
```

Or fetch headers at build time:

```makefile
STB_ME = https://raw.githubusercontent.com/nhlmg93/stb_me/master

vendor/stb_me/json.h:
	mkdir -p vendor/stb_me
	curl -fsSL $(STB_ME)/json.h -o $@
```

## License

| File | License |
|------|---------|
| `json.h` | Public domain (Unlicense), from upstream |
| `clay.h` | zlib, Copyright (c) Nic Barker |
