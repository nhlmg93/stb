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

## Usage

Add this repo to your include path (or vendor it as a submodule) and include headers directly:

```bash
git submodule add https://github.com/nhlmg93/stb_me.git vendor/stb_me
```

```makefile
CFLAGS += -Ivendor/stb_me
```

## License

`json.h` is in the public domain (Unlicense), inherited from the upstream project.
