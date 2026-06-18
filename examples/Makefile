CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I..
LDFLAGS = -lssl -lcrypto

test_smoke: test_smoke.c ../test.h
	$(CC) $(CFLAGS) -o $@ test_smoke.c

http_test: http_test.c ../http.h ../test.h
	$(CC) $(CFLAGS) -o $@ http_test.c $(LDFLAGS)

clay_term_demo: clay_term_demo.c ../clay.h ../clay_term.h ../termbox2.h
	$(CC) $(CFLAGS) -o $@ clay_term_demo.c

test: test_smoke http_test
	./test_smoke
	./http_test

clean:
	rm -f test_smoke http_test clay_term_demo

.PHONY: clean test clay_term_demo
