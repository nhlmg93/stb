#define _DEFAULT_SOURCE
#define HTTP_IMPLEMENTATION
#include "../http.h"

#define TEST_IMPLEMENTATION
#include "../test.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_port;
static pid_t g_server_pid;

static void http_drain(http_t *h) {
    while (http_process(h) == HTTP_STATUS_PENDING) { }
}

static char *url(char *buf, size_t cap, const char *path) {
    snprintf(buf, cap, "http://127.0.0.1:%d%s", g_port, path);
    return buf;
}

static int body_contains(http_t *h, const char *needle) {
    if (!h->response_data || h->response_size == 0) return 0;
    {
        const char *body = (const char *)h->response_data;
        size_t i;
        size_t n = strlen(needle);
        if (n == 0) return 1;
        if (h->response_size < n) return 0;
        for (i = 0; i + n <= h->response_size; i++) {
            if (memcmp(body + i, needle, n) == 0) return 1;
        }
    }
    return 0;
}

static void write_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, data, len, 0);
        if (n <= 0) return;
        data += n;
        len -= (size_t)n;
    }
}

static void respond(int fd, int code, const char *type, const char *body) {
    char hdr[512];
    size_t body_len = body ? strlen(body) : 0;
    int hdr_len = snprintf(hdr, sizeof hdr,
                           "HTTP/1.1 %d OK\r\n"
                           "Content-Type: %s\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n\r\n",
                           code, type, body_len);
    write_all(fd, hdr, (size_t)hdr_len);
    if (body_len > 0) write_all(fd, body, body_len);
}

static int read_request(int fd, char *req, size_t cap, size_t *out_len) {
    size_t total = 0;
    char *hdr_end;

    while (total + 1 < cap) {
        ssize_t n = recv(fd, req + total, (int)(cap - 1 - total), 0);
        if (n <= 0) return -1;
        total += (size_t)n;
        req[total] = 0;
        hdr_end = strstr(req, "\r\n\r\n");
        if (hdr_end) {
            size_t hdr_bytes = (size_t)(hdr_end - req) + 4;
            size_t content_length = 0;
            char *cl = strstr(req, "Content-Length:");
            if (cl) {
                cl += 15;
                while (*cl == ' ') cl++;
                content_length = (size_t)strtoull(cl, NULL, 10);
            }
            while (total < hdr_bytes + content_length && total + 1 < cap) {
                n = recv(fd, req + total, (int)(cap - 1 - total), 0);
                if (n <= 0) return -1;
                total += (size_t)n;
                req[total] = 0;
            }
            *out_len = total;
            return 0;
        }
    }
    return -1;
}

static void handle_client(int fd) {
    char req[8192];
    size_t req_len = 0;
    const char *path;
    const char *path_end;
    size_t path_len;
    const char *body_start;
    size_t body_len = 0;
    const char *hdr;

    if (read_request(fd, req, sizeof req, &req_len) != 0) {
        close(fd);
        return;
    }

    path = strchr(req, ' ');
    if (!path) {
        close(fd);
        return;
    }
    path++;
    path_end = strchr(path, ' ');
    if (!path_end) {
        close(fd);
        return;
    }
    path_len = (size_t)(path_end - path);

    body_start = strstr(req, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        body_len = req_len - (size_t)(body_start - req);
    }

    hdr = strstr(req, "X-Smoke-Test:");

    if (path_len == 1 && path[0] == '/') {
        respond(fd, 200, "text/plain", "hello http");
    } else if (path_len == 5 && memcmp(path, "/echo", 5) == 0) {
        char resp[4096];
        if (hdr) {
            const char *hdr_val = hdr + 13;
            const char *end = hdr_val;
            while (*hdr_val == ' ') hdr_val++;
            end = hdr_val;
            while (*end && *end != '\r' && *end != '\n') end++;
            snprintf(resp, sizeof resp, "header=%.*s\nbody=%.*s", (int)(end - hdr_val), hdr_val,
                     (int)body_len, body_start ? body_start : "");
        } else {
            snprintf(resp, sizeof resp, "header=\nbody=%.*s", (int)body_len, body_start ? body_start : "");
        }
        respond(fd, 200, "text/plain", resp);
    } else if (path_len == 4 && memcmp(path, "/sse", 4) == 0) {
        const char *payload =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Connection: close\r\n\r\n"
            "event: ping\r\n"
            "data: hello sse\r\n\r\n";
        write_all(fd, payload, strlen(payload));
    } else if (path_len == 4 && memcmp(path, "/big", 4) == 0) {
        char hdr[256];
        char block[1024];
        int i;
        int hdr_len = snprintf(hdr, sizeof hdr,
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 8192\r\n"
                               "Connection: close\r\n\r\n");
        memset(block, 'x', sizeof block);
        write_all(fd, hdr, (size_t)hdr_len);
        for (i = 0; i < 8; i++) write_all(fd, block, sizeof block);
    } else {
        respond(fd, 404, "text/plain", "not found");
    }

    close(fd);
}

static void run_server(int write_fd) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof addr;
    char port_buf[16];

    if (listen_fd < 0) _exit(1);

    {
        int yes = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0) _exit(1);
    if (listen(listen_fd, 8) < 0) _exit(1);
    if (getsockname(listen_fd, (struct sockaddr *)&addr, &addr_len) < 0) _exit(1);

    snprintf(port_buf, sizeof port_buf, "%d", ntohs(addr.sin_port));
    (void)write(write_fd, port_buf, strlen(port_buf));
    close(write_fd);

    for (;;) {
        int client = accept(listen_fd, NULL, NULL);
        if (client < 0) continue;
        handle_client(client);
    }
}

static int start_server(void) {
    int pipefd[2];
    char port_buf[16];
    ssize_t n;

    if (pipe(pipefd) != 0) return -1;
    g_server_pid = fork();
    if (g_server_pid < 0) return -1;
    if (g_server_pid == 0) {
        close(pipefd[0]);
        run_server(pipefd[1]);
        _exit(0);
    }
    close(pipefd[1]);
    n = read(pipefd[0], port_buf, sizeof port_buf - 1);
    close(pipefd[0]);
    if (n <= 0) return -1;
    port_buf[n] = 0;
    g_port = atoi(port_buf);
    usleep(50000);
    return g_port > 0 ? 0 : -1;
}

static void stop_server(void) {
    if (g_server_pid > 0) {
        kill(g_server_pid, SIGTERM);
        waitpid(g_server_pid, NULL, 0);
        g_server_pid = 0;
    }
}

TEST(get_http) {
    char u[128];
    http_t *h = http_get(url(u, sizeof u, "/"), NULL);
    ASSERT_NOT_NULL(h);
    http_drain(h);
    ASSERT_EQ(h->status, HTTP_STATUS_COMPLETED);
    ASSERT_EQ(h->status_code, 200);
    ASSERT_STR_CONTAINS((const char *)h->response_data, "hello http");
    http_release(h);
    return TEST_OK;
}

TEST(post_http) {
    char u[128];
    const char *body = "{\"smoke\":true}";
    http_t *h = http_post(url(u, sizeof u, "/echo"), body, strlen(body), NULL);
    ASSERT_NOT_NULL(h);
    http_header(h, "Content-Type", "application/json");
    http_drain(h);
    ASSERT_EQ(h->status, HTTP_STATUS_COMPLETED);
    ASSERT_STR_CONTAINS((const char *)h->response_data, "body={\"smoke\":true}");
    http_release(h);
    return TEST_OK;
}

TEST(custom_header) {
    char u[128];
    http_t *h = http_get(url(u, sizeof u, "/echo"), NULL);
    ASSERT_NOT_NULL(h);
    http_header(h, "X-Smoke-Test", "1");
    http_drain(h);
    ASSERT_EQ(h->status, HTTP_STATUS_COMPLETED);
    ASSERT_STR_CONTAINS((const char *)h->response_data, "header=1");
    http_release(h);
    return TEST_OK;
}

typedef struct {
    size_t bytes;
    int chunks;
} chunk_ctx;

static void on_chunk(void *ctx, const char *data, size_t len) {
    chunk_ctx *c = (chunk_ctx *)ctx;
    c->bytes += len;
    c->chunks++;
    (void)data;
}

TEST(chunk_callback) {
    char u[128];
    chunk_ctx ctx = {0, 0};
    http_t *h = http_get(url(u, sizeof u, "/big"), NULL);
    ASSERT_NOT_NULL(h);
    http_on_chunk(h, on_chunk, &ctx);
    http_drain(h);
    ASSERT_EQ(h->status, HTTP_STATUS_COMPLETED);
    ASSERT_EQ(h->response_size, 8192u);
    ASSERT_EQ(ctx.bytes, 8192u);
    ASSERT(ctx.chunks > 0);
    http_release(h);
    return TEST_OK;
}

static int sse_got_event;

static void on_sse(void *ctx, const http_sse_event_t *ev) {
    (void)ctx;
    if (ev->data_len > 0) sse_got_event = 1;
}

TEST(sse_get) {
    char u[128];
    http_t *h = http_get(url(u, sizeof u, "/sse"), NULL);
    ASSERT_NOT_NULL(h);
    sse_got_event = 0;
    http_sse(h, 1);
    http_on_sse(h, on_sse, NULL);
    while (http_process(h) == HTTP_STATUS_PENDING && !sse_got_event) { }
    ASSERT(sse_got_event);
    http_release(h);
    return TEST_OK;
}

TEST(sse_post) {
    char u[128];
    const char *body = "stream=true";
    http_t *h = http_post(url(u, sizeof u, "/sse"), body, strlen(body), NULL);
    ASSERT_NOT_NULL(h);
    http_header(h, "Content-Type", "text/plain");
    sse_got_event = 0;
    http_sse(h, 1);
    http_on_sse(h, on_sse, NULL);
    while (http_process(h) == HTTP_STATUS_PENDING && !sse_got_event) { }
    ASSERT(sse_got_event);
    http_release(h);
    return TEST_OK;
}

TEST(get_https) {
    http_t *h;
    SKIP_IF(!getenv("HTTP_TEST_NETWORK"), "set HTTP_TEST_NETWORK=1 for external HTTPS tests");
    h = http_get("https://example.com/", NULL);
    ASSERT_NOT_NULL(h);
    http_drain(h);
    ASSERT_EQ(h->status, HTTP_STATUS_COMPLETED);
    ASSERT_EQ(h->status_code, 200);
    ASSERT(h->response_size > 0);
    http_release(h);
    return TEST_OK;
}

TEST(post_https) {
    const char *body = "{\"smoke\":true}";
    http_t *h;
    SKIP_IF(!getenv("HTTP_TEST_NETWORK"), "set HTTP_TEST_NETWORK=1 for external HTTPS tests");
    h = http_post("https://postman-echo.com/post", body, strlen(body), NULL);
    ASSERT_NOT_NULL(h);
    http_header(h, "Content-Type", "application/json");
    http_drain(h);
    ASSERT_EQ(h->status, HTTP_STATUS_COMPLETED);
    ASSERT(body_contains(h, "\"smoke\":true") || body_contains(h, "\"smoke\": true"));
    http_release(h);
    return TEST_OK;
}

int main(void) {
    const test_case cases[] = {
        TEST_ENTRY(get_http),
        TEST_ENTRY(post_http),
        TEST_ENTRY(custom_header),
        TEST_ENTRY(chunk_callback),
        TEST_ENTRY(sse_get),
        TEST_ENTRY(sse_post),
        TEST_ENTRY(get_https),
        TEST_ENTRY(post_https),
    };
    int rc;

    if (start_server() != 0) {
        fprintf(stderr, "failed to start local test server\n");
        return 1;
    }

    rc = test_run(cases, (int)(sizeof cases / sizeof cases[0]));
    stop_server();
    return rc;
}
