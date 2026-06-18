CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -I..
LDFLAGS = -lssl -lcrypto

TESTS = test_smoke json_test stb_ds_test minicoro_test clay_test termbox_test http_test clay_term_test clay_raylib_test

test_smoke: test_smoke.c ../munit.h
	$(CC) $(CFLAGS) -o $@ test_smoke.c

json_test: json_test.c ../json.h ../munit.h
	$(CC) $(CFLAGS) -o $@ json_test.c

stb_ds_test: stb_ds_test.c ../stb_ds.h ../munit.h
	$(CC) $(CFLAGS) -o $@ stb_ds_test.c

minicoro_test: minicoro_test.c ../minicoro.h ../munit.h
	$(CC) $(CFLAGS) -o $@ minicoro_test.c

clay_test: clay_test.c ../clay.h ../munit.h
	$(CC) $(CFLAGS) -o $@ clay_test.c

termbox_test: termbox_test.c ../termbox2.h ../munit.h
	$(CC) $(CFLAGS) -o $@ termbox_test.c

http_test: http_test.c ../http.h ../munit.h
	$(CC) $(CFLAGS) -o $@ http_test.c $(LDFLAGS)

clay_term_test: clay_term_test.c ../clay.h ../clay_term.h ../munit.h
	$(CC) $(CFLAGS) -o $@ clay_term_test.c

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LDFLAGS := $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(RAYLIB_LDFLAGS),)
RAYLIB_LDFLAGS = -lraylib
endif

clay_raylib_test: clay_raylib_test.c ../clay.h ../clay_raylib.h ../munit.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o $@ clay_raylib_test.c $(RAYLIB_LDFLAGS) -lm

test: $(TESTS)
	@for t in $(TESTS); do \
		echo "==> $$t"; \
		./$$t || exit 1; \
	done

clean:
	rm -f $(TESTS) munit_smoke

.PHONY: clean test
