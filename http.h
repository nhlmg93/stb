/*
 * http.h — stb
 *
 * Poll-based HTTP/HTTPS client with streaming body and SSE support.
 * Extends the socket/poll model from http.h by Mattias Gustavsson —
 * https://github.com/mattiasgustavsson/libs (not a dependency; code ported here).
 *
 *   #define HTTP_IMPLEMENTATION
 *   #include "http.h"
 *
 * HTTPS (when HTTP_HTTPS is 1) uses OpenSSL/libssl on the system — link -lssl -lcrypto.
 *
 * Arena alloc — pass your arena as memctx to http_get/http_post:
 *
 *   #define HTTP_MALLOC(ctx, n) my_arena_alloc(ctx, n)
 *   #define HTTP_FREE(ctx, p)   ((void)0)
 *   #define HTTP_IMPLEMENTATION
 *   #include "http.h"
 *
 * HTTP-only build: #define HTTP_HTTPS 0 before include.
 */

#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <stdint.h>

typedef enum http_status_t {
    HTTP_STATUS_PENDING,
    HTTP_STATUS_COMPLETED,
    HTTP_STATUS_FAILED
} http_status_t;

typedef struct http_sse_event {
    const char *data;
    size_t data_len;
    const char *event;
    size_t event_len;
} http_sse_event_t;

typedef void (*http_chunk_fn)(void *ctx, const char *data, size_t len);
typedef void (*http_sse_fn)(void *ctx, const http_sse_event_t *ev);

typedef struct http_t {
    http_status_t status;
    int status_code;
    char const *reason_phrase;
    char const *content_type;
    size_t response_size;
    void *response_data;
} http_t;

http_t *http_get(const char *url, void *memctx);
http_t *http_post(const char *url, const void *body, size_t body_len, void *memctx);
void http_header(http_t *w, const char *name, const char *value);
void http_sse(http_t *w, int enable);
void http_on_chunk(http_t *w, http_chunk_fn fn, void *ctx);
void http_on_sse(http_t *w, http_sse_fn fn, void *ctx);
http_status_t http_process(http_t *w);
void http_release(http_t *w);

#endif /* HTTP_H */

#ifdef HTTP_IMPLEMENTATION

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#ifndef HTTP_MALLOC
#include <stdlib.h>
#define HTTP_MALLOC(ctx, size) ((void)(ctx), malloc(size))
#define HTTP_FREE(ctx, ptr) free(ptr)
#endif

#ifndef HTTP_CA_FILE
#define HTTP_CA_FILE "/etc/ssl/certs/ca-certificates.crt"
#endif

#ifndef HTTP_TLS_INSECURE
#define HTTP_TLS_INSECURE 0
#endif

#ifndef HTTP_HTTPS
#define HTTP_HTTPS 1
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#define HTTP_SOCKET SOCKET
#define HTTP_INVALID_SOCKET INVALID_SOCKET
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#define HTTP_SOCKET int
#define HTTP_INVALID_SOCKET (-1)
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if HTTP_HTTPS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#define HTTP_MAX_HEADERS 32
#define HTTP_HEADER_NAME 128
#define HTTP_HEADER_VALUE 1024
#define HTTP_INITIAL_CAP (64 * 1024)

typedef struct http_header_pair {
    char name[HTTP_HEADER_NAME];
    char value[HTTP_HEADER_VALUE];
} http_header_pair;

typedef struct http_sse_state {
    char line[4096];
    size_t line_len;
    char data[65536];
    size_t data_len;
    char event[256];
    size_t event_len;
} http_sse_state;

typedef struct http_internal {
    http_t http;
    void *memctx;

    HTTP_SOCKET socket;
    int https;
    int connect_pending;
    int request_sent;
    int headers_done;

    char address[256];
    char port[16];
    char host_header[256];

    char method[8];
    char resource[2048];

    char request_buf[8192];
    size_t request_len;
    size_t request_sent_bytes;

    const void *post_body;
    size_t post_body_len;
    size_t post_body_sent;

    char reason_phrase[1024];
    char content_type[256];

    size_t data_size;
    size_t data_capacity;
    char *data;
    size_t hdr_size;

    int chunked;
    size_t chunk_remaining;
    size_t content_length;
    int has_content_length;

    http_header_pair headers[HTTP_MAX_HEADERS];
    int header_count;

    int sse_mode;
    http_chunk_fn chunk_fn;
    void *chunk_ctx;
    http_sse_fn sse_fn;
    void *sse_ctx;
    http_sse_state sse;

#if HTTP_HTTPS
    SSL *ssl;
    int tls_ready;
#endif
} http_internal;

#if HTTP_HTTPS
static SSL_CTX *http_ssl_ctx;
static int http_ssl_inited;

static int http_ssl_global_init(void) {
    if (http_ssl_inited) return http_ssl_ctx != NULL;
    http_ssl_inited = 1;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif

    http_ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!http_ssl_ctx) return 0;

    SSL_CTX_set_min_proto_version(http_ssl_ctx, TLS1_2_VERSION);

#if HTTP_TLS_INSECURE
    SSL_CTX_set_verify(http_ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
    SSL_CTX_set_verify(http_ssl_ctx, SSL_VERIFY_PEER, NULL);
    if (!SSL_CTX_set_default_verify_paths(http_ssl_ctx)) {
        SSL_CTX_load_verify_locations(http_ssl_ctx, HTTP_CA_FILE, NULL);
    }
#endif
    return 1;
}

static int http_tls_setup(http_internal *w) {
    if (!http_ssl_global_init()) return 0;
    w->ssl = SSL_new(http_ssl_ctx);
    if (!w->ssl) return 0;
    SSL_set_fd(w->ssl, (int)w->socket);
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    SSL_set_tlsext_host_name(w->ssl, w->host_header);
#endif
    SSL_set_connect_state(w->ssl);
    return 1;
}

static int http_tls_handshake(http_internal *w) {
    int r = SSL_do_handshake(w->ssl);
    if (r == 1) return 1;
    {
        int err = SSL_get_error(w->ssl, r);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
    }
    return -1;
}

static void http_tls_shutdown(http_internal *w) {
    if (w->ssl) {
        SSL_shutdown(w->ssl);
        SSL_free(w->ssl);
        w->ssl = NULL;
    }
}
#endif /* HTTP_HTTPS */

static int http_parse_url(const char *url, int *https, char *address, size_t acap, char *port, size_t pcap,
                         const char **resource) {
    const char *p = url;
    *https = 0;
    if (strncmp(p, "https://", 8) == 0) {
        *https = 1;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else {
        return 0;
    }

    {
        const char *end = strchr(p, ':');
        if (!end) end = strchr(p, '/');
        if (!end) end = p + strlen(p);
        size_t n = (size_t)(end - p);
        if (n >= acap) return 0;
        memcpy(address, p, n);
        address[n] = 0;
        p = end;
    }

    if (*p == ':') {
        const char *end = strchr(++p, '/');
        if (!end) end = p + strlen(p);
        size_t n = (size_t)(end - p);
        if (n >= pcap) return 0;
        memcpy(port, p, n);
        port[n] = 0;
        p = end;
    } else {
        strcpy(port, *https ? "443" : "80");
    }

    *resource = (*p) ? p : "/";
    return 1;
}

static HTTP_SOCKET http_connect(const char *address, const char *port) {
    struct addrinfo hints, *res = NULL, *it;
    HTTP_SOCKET sock = HTTP_INVALID_SOCKET;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(address, port, &hints, &res) != 0) return HTTP_INVALID_SOCKET;

    for (it = res; it; it = it->ai_next) {
        int flags;
        sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == HTTP_INVALID_SOCKET) continue;
#ifdef _WIN32
        u_long nb = 1;
        ioctlsocket(sock, FIONBIO, &nb);
#else
        flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        if (connect(sock, it->ai_addr, (int)it->ai_addrlen) == 0) break;
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK && WSAGetLastError() != WSAEINPROGRESS) {
            closesocket(sock);
            sock = HTTP_INVALID_SOCKET;
            continue;
        }
#else
        if (errno != EINPROGRESS && errno != EWOULDBLOCK && errno != EAGAIN) {
            close(sock);
            sock = HTTP_INVALID_SOCKET;
            continue;
        }
#endif
        break;
    }
    freeaddrinfo(res);
    return sock;
}

static http_internal *http_create(void *memctx) {
    http_internal *w = (http_internal *)HTTP_MALLOC(memctx, sizeof(http_internal));
    memset(w, 0, sizeof *w);
    w->memctx = memctx;
    w->http.status = HTTP_STATUS_PENDING;
    w->http.reason_phrase = w->reason_phrase;
    w->http.content_type = w->content_type;
    w->socket = HTTP_INVALID_SOCKET;
    w->connect_pending = 1;
    w->data_capacity = HTTP_INITIAL_CAP;
    w->data = (char *)HTTP_MALLOC(memctx, w->data_capacity);
    return w;
}

static void http_build_request(http_internal *w, const char *method, const char *resource) {
    size_t n = 0;
    int default_port = (strcmp(w->port, w->https ? "443" : "80") == 0);

    n += (size_t)snprintf(w->request_buf + n, sizeof w->request_buf - n, "%s %s HTTP/1.1\r\nHost: %s", method,
                          resource, w->address);
    if (!default_port) n += (size_t)snprintf(w->request_buf + n, sizeof w->request_buf - n, ":%s", w->port);
    n += (size_t)snprintf(w->request_buf + n, sizeof w->request_buf - n, "\r\n");

    if (w->sse_mode) {
        n += (size_t)snprintf(w->request_buf + n, sizeof w->request_buf - n, "Accept: text/event-stream\r\n");
    }

    {
        int i;
        for (i = 0; i < w->header_count; i++) {
            n += (size_t)snprintf(w->request_buf + n, sizeof w->request_buf - n, "%s: %s\r\n", w->headers[i].name,
                                  w->headers[i].value);
        }
    }

    if (w->post_body_len > 0) {
        n += (size_t)snprintf(w->request_buf + n, sizeof w->request_buf - n, "Content-Length: %zu\r\n",
                              w->post_body_len);
    }

    n += (size_t)snprintf(w->request_buf + n, sizeof w->request_buf - n, "Connection: close\r\n\r\n");
    w->request_len = n;
}

static int http_grow(http_internal *w, size_t need) {
    if (w->data_size + need + 1 <= w->data_capacity) return 1;
    size_t cap = w->data_capacity;
    while (cap < w->data_size + need + 1) cap *= 2;
    char *p = (char *)HTTP_MALLOC(w->memctx, cap);
    if (!p) return 0;
    memcpy(p, w->data, w->data_size);
    HTTP_FREE(w->memctx, w->data);
    w->data = p;
    w->data_capacity = cap;
    return 1;
}

static void http_dispatch_body(http_internal *w, const char *bytes, size_t len) {
    if (len == 0) return;
    if (w->chunk_fn) w->chunk_fn(w->chunk_ctx, bytes, len);
    if (w->sse_mode && w->sse_fn) {
        size_t i;
        for (i = 0; i < len; i++) {
            char c = bytes[i];
            http_sse_state *s = &w->sse;
            if (c == '\n') {
                s->line[s->line_len] = 0;
                if (s->line_len == 0) {
                    if (s->data_len > 0) {
                        http_sse_event_t ev;
                        s->data[s->data_len] = 0;
                        ev.data = s->data;
                        ev.data_len = s->data_len;
                        ev.event = s->event_len ? s->event : NULL;
                        ev.event_len = s->event_len;
                        if (!(s->data_len == 6 && memcmp(s->data, "[DONE]", 6) == 0)) {
                            w->sse_fn(w->sse_ctx, &ev);
                        }
                        s->data_len = 0;
                        s->event_len = 0;
                    }
                } else if (strncmp(s->line, "data:", 5) == 0) {
                    const char *d = s->line + 5;
                    if (*d == ' ') d++;
                    {
                        size_t dl = strlen(d);
                        if (s->data_len + dl + 1 < sizeof s->data) {
                            if (s->data_len) s->data[s->data_len++] = '\n';
                            memcpy(s->data + s->data_len, d, dl);
                            s->data_len += dl;
                        }
                    }
                } else if (strncmp(s->line, "event:", 6) == 0) {
                    const char *e = s->line + 6;
                    if (*e == ' ') e++;
                    s->event_len = strlen(e);
                    if (s->event_len >= sizeof s->event) s->event_len = sizeof s->event - 1;
                    memcpy(s->event, e, s->event_len);
                    s->event[s->event_len] = 0;
                }
                s->line_len = 0;
            } else if (c != '\r') {
                if (s->line_len + 1 < sizeof s->line) s->line[s->line_len++] = c;
            }
        }
    }
    if (http_grow(w, len)) {
        memcpy(w->data + w->data_size, bytes, len);
        w->data_size += len;
        w->data[w->data_size] = 0;
    }
}

static int http_io_read(http_internal *w, char *buf, size_t cap) {
#if HTTP_HTTPS
    if (w->https && w->ssl) {
        int n = SSL_read(w->ssl, buf, (int)cap);
        if (n > 0) return n;
        if (n == 0) return -1;
        {
            int err = SSL_get_error(w->ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
        }
        return -1;
    }
#endif
    {
        int n = (int)recv(w->socket, buf, (int)cap, 0);
        if (n == 0) return -1;
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
#endif
            return -1;
        }
        return n;
    }
}

static int http_io_write(http_internal *w, const char *buf, size_t len) {
#if HTTP_HTTPS
    if (w->https && w->ssl) {
        int n = SSL_write(w->ssl, buf, (int)len);
        if (n > 0) return n;
        if (n == 0) return 0;
        {
            int err = SSL_get_error(w->ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
        }
        return -1;
    }
#endif
    {
        int n = (int)send(w->socket, buf, (int)len, 0);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
#endif
            return -1;
        }
        return n;
    }
}

static size_t http_header_size(http_internal *w) {
    char *p = strstr(w->data, "\r\n\r\n");
    return p ? (size_t)(p - w->data) + 4 : 0;
}

static void http_parse_headers(http_internal *w) {
    char *p = w->data;
    char *end = strstr(p, "\r\n\r\n");
    char *line;
    if (!end) return;
    *end = 0;
    line = strchr(p, ' ');
    if (line) {
        line++;
        w->http.status_code = atoi(line);
        line = strchr(line, ' ');
        if (line) {
            line++;
            {
                char *rp = strchr(line, '\r');
                if (rp) {
                    size_t n = (size_t)(rp - line);
                    if (n >= sizeof w->reason_phrase) n = sizeof w->reason_phrase - 1;
                    memcpy(w->reason_phrase, line, n);
                    w->reason_phrase[n] = 0;
                }
            }
        }
    }
    for (line = strchr(p, '\n'); line; line = strchr(line + 1, '\n')) {
        line++;
        if (strncmp(line, "Content-Type:", 13) == 0) {
            const char *v = line + 13;
            if (*v == ' ') v++;
            {
                char *e = strchr(v, '\r');
                size_t n = e ? (size_t)(e - v) : strlen(v);
                if (n >= sizeof w->content_type) n = sizeof w->content_type - 1;
                memcpy(w->content_type, v, n);
                w->content_type[n] = 0;
            }
        } else if (strncmp(line, "Transfer-Encoding:", 18) == 0) {
            if (strstr(line, "chunked")) w->chunked = 1;
        } else if (strncmp(line, "Content-Length:", 15) == 0) {
            w->content_length = (size_t)atoll(line + 15);
            w->has_content_length = 1;
        }
    }
    w->headers_done = 1;
}

http_t *http_get(const char *url, void *memctx) {
    http_internal *w = http_create(memctx);
    const char *resource;
    if (!http_parse_url(url, &w->https, w->address, sizeof w->address, w->port, sizeof w->port, &resource)) {
        HTTP_FREE(memctx, w);
        return NULL;
    }
#if HTTP_HTTPS
    if (w->https && !http_ssl_global_init()) {
        HTTP_FREE(memctx, w->data);
        HTTP_FREE(memctx, w);
        return NULL;
    }
#else
    if (w->https) {
        HTTP_FREE(memctx, w->data);
        HTTP_FREE(memctx, w);
        return NULL;
    }
#endif
    snprintf(w->host_header, sizeof w->host_header, "%s", w->address);
    w->socket = http_connect(w->address, w->port);
    if (w->socket == HTTP_INVALID_SOCKET) {
        HTTP_FREE(memctx, w->data);
        HTTP_FREE(memctx, w);
        return NULL;
    }
    strcpy(w->method, "GET");
    snprintf(w->resource, sizeof w->resource, "%s", resource);
    return &w->http;
}

http_t *http_post(const char *url, const void *body, size_t body_len, void *memctx) {
    http_internal *w = http_create(memctx);
    const char *resource;
    if (!http_parse_url(url, &w->https, w->address, sizeof w->address, w->port, sizeof w->port, &resource)) {
        HTTP_FREE(memctx, w);
        return NULL;
    }
#if HTTP_HTTPS
    if (w->https && !http_ssl_global_init()) {
        HTTP_FREE(memctx, w->data);
        HTTP_FREE(memctx, w);
        return NULL;
    }
#else
    if (w->https) {
        HTTP_FREE(memctx, w->data);
        HTTP_FREE(memctx, w);
        return NULL;
    }
#endif
    snprintf(w->host_header, sizeof w->host_header, "%s", w->address);
    w->post_body = body;
    w->post_body_len = body_len;
    w->socket = http_connect(w->address, w->port);
    if (w->socket == HTTP_INVALID_SOCKET) {
        HTTP_FREE(memctx, w->data);
        HTTP_FREE(memctx, w);
        return NULL;
    }
    strcpy(w->method, "POST");
    snprintf(w->resource, sizeof w->resource, "%s", resource);
    return &w->http;
}

void http_header(http_t *http, const char *name, const char *value) {
    http_internal *w = (http_internal *)http;
    if (w->header_count >= HTTP_MAX_HEADERS) return;
    snprintf(w->headers[w->header_count].name, HTTP_HEADER_NAME, "%s", name);
    snprintf(w->headers[w->header_count].value, HTTP_HEADER_VALUE, "%s", value);
    w->header_count++;
}

void http_sse(http_t *http, int enable) {
    ((http_internal *)http)->sse_mode = enable ? 1 : 0;
}

void http_on_chunk(http_t *http, http_chunk_fn fn, void *ctx) {
    http_internal *w = (http_internal *)http;
    w->chunk_fn = fn;
    w->chunk_ctx = ctx;
}

void http_on_sse(http_t *http, http_sse_fn fn, void *ctx) {
    http_internal *w = (http_internal *)http;
    w->sse_fn = fn;
    w->sse_ctx = ctx;
}

http_status_t http_process(http_t *http) {
    http_internal *w = (http_internal *)http;
    fd_set rfds, wfds;
    struct timeval tv;
    char buf[4096];
    int n;

    if (http->status != HTTP_STATUS_PENDING) return http->status;

    if (w->connect_pending) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(w->socket, &wfds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        if (select(w->socket + 1, NULL, &wfds, NULL, &tv) == 1) {
            int err = 0;
            socklen_t el = sizeof err;
            getsockopt(w->socket, SOL_SOCKET, SO_ERROR, (char *)&err, &el);
            if (err == 0) w->connect_pending = 0;
        }
        if (w->connect_pending) return HTTP_STATUS_PENDING;
    }

#if HTTP_HTTPS
    if (w->https && !w->tls_ready) {
        if (!w->ssl) {
            if (!http_tls_setup(w)) {
                http->status = HTTP_STATUS_FAILED;
                return HTTP_STATUS_FAILED;
            }
        }
        n = http_tls_handshake(w);
        if (n < 0) {
            http->status = HTTP_STATUS_FAILED;
            return HTTP_STATUS_FAILED;
        }
        if (n == 0) return HTTP_STATUS_PENDING;
        w->tls_ready = 1;
    }
#endif

    if (!w->request_sent) {
        if (w->request_len == 0) http_build_request(w, w->method, w->resource);

        size_t left = w->request_len - w->request_sent_bytes;
        if (left > 0) {
            n = http_io_write(w, w->request_buf + w->request_sent_bytes, left);
            if (n < 0) {
                http->status = HTTP_STATUS_FAILED;
                return HTTP_STATUS_FAILED;
            }
            if (n == 0) return HTTP_STATUS_PENDING;
            w->request_sent_bytes += (size_t)n;
            if (w->request_sent_bytes < w->request_len) return HTTP_STATUS_PENDING;
        }
        if (w->post_body_len > 0) {
            left = w->post_body_len - w->post_body_sent;
            n = http_io_write(w, (const char *)w->post_body + w->post_body_sent, left);
            if (n < 0) {
                http->status = HTTP_STATUS_FAILED;
                return HTTP_STATUS_FAILED;
            }
            if (n == 0) return HTTP_STATUS_PENDING;
            w->post_body_sent += (size_t)n;
            if (w->post_body_sent < w->post_body_len) return HTTP_STATUS_PENDING;
        }
        w->request_sent = 1;
    }

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(w->socket, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (select(w->socket + 1, &rfds, NULL, NULL, &tv) != 1) return HTTP_STATUS_PENDING;

    n = http_io_read(w, buf, sizeof buf);
    if (n < 0) {
        if (!w->headers_done) {
            http->status = HTTP_STATUS_FAILED;
            return HTTP_STATUS_FAILED;
        }
        goto finish;
    }
    if (n == 0) {
        if (!w->request_sent) return HTTP_STATUS_PENDING;
        goto finish;
    }

    if (!w->headers_done) {
        if (!http_grow(w, (size_t)n)) {
            http->status = HTTP_STATUS_FAILED;
            return HTTP_STATUS_FAILED;
        }
        memcpy(w->data + w->data_size, buf, (size_t)n);
        w->data_size += (size_t)n;
        w->data[w->data_size] = 0;
        if (strstr(w->data, "\r\n\r\n")) {
            size_t hdr = http_header_size(w);
            size_t initial_body = 0;
            http_parse_headers(w);
            w->hdr_size = hdr;
            if (w->data_size > hdr) initial_body = w->data_size - hdr;
            w->data_size = hdr;
            if (initial_body > 0) http_dispatch_body(w, w->data + hdr, initial_body);
        }
        return HTTP_STATUS_PENDING;
    }

    http_dispatch_body(w, buf, (size_t)n);
    return HTTP_STATUS_PENDING;

finish:
    {
        http->response_data = w->data + w->hdr_size;
        http->response_size = w->data_size > w->hdr_size ? w->data_size - w->hdr_size : 0;
        if (http->response_data && http->response_size > 0)
            ((char *)http->response_data)[http->response_size] = 0;
    }
    http->status = (http->status_code > 0 && http->status_code < 400) ? HTTP_STATUS_COMPLETED : HTTP_STATUS_FAILED;
    return http->status;
}

void http_release(http_t *http) {
    http_internal *w = (http_internal *)http;
#if HTTP_HTTPS
    http_tls_shutdown(w);
#endif
    if (w->socket != HTTP_INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(w->socket);
#else
        close(w->socket);
#endif
    }
    HTTP_FREE(w->memctx, w->data);
    HTTP_FREE(w->memctx, w);
}

#endif /* HTTP_IMPLEMENTATION */

/*
 * http.h — MIT License (socket/poll portions derived from http.h by Mattias Gustavsson)
 *
 * Copyright (c) Mattias Gustavsson — underlying HTTP client design (http.h)
 * Copyright (c) 2026 stb contributors — HTTPS glue, SSE, and extensions in this file
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
