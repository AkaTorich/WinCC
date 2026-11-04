// A small web server: its own thread waits for connections; each connection gets a
// thread of its own that reads the request (head and body), so a slow client keeps nobody
// waiting. The handler is called by one connection at a time (under a lock): it needs no
// care for threads. What it answered (or 404) is sent and the connection closes.
#include "net_int.h"

#define CONN_MAX 64                                 // connections served at once; the next ones wait

struct NetHttpServer {
    NetSocket *listen;
    NetHttpHandler handler;
    void *user;
    HANDLE thread;                                  // 0: net_http_run serves in its own thread
    DWORD thread_id;
    volatile int stopping;                          // 1: stopped from outside; 2: by its own handler
    CRITICAL_SECTION handler_lock;                  // one handler at a time
    CRITICAL_SECTION conns_lock;
    NetSocket *conns[CONN_MAX];                     // the connections being served (stop cuts them)
    NetSocket *answering;                           // the one whose handler runs now: stop lets it finish
    volatile LONG active;
};

static _Thread_local NetHttpServer *g_in_handler;   // the server whose handler runs in this thread

typedef struct {
    NetHttpServer *server;
    NetSocket *conn;
    int replied;
    NetBuf extra;                                   // net_http_add_header
    char **strings;                                 // net_http_param, net_http_header
    int count;
} Request;

#define HEAD_LIMIT (64 * 1024)
#define BODY_LIMIT (64 * 1024 * 1024)

static const char *reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    }
    return status < 400 ? "OK" : "Error";
}

static const char *keep(Request *q, char *s)        // the request owns s from now on
{
    if (!s)
        return 0;
    char **list = realloc(q->strings, sizeof(char *) * (size_t)(q->count + 1));
    if (!list) {
        free(s);
        return 0;
    }
    q->strings = list;
    q->strings[q->count++] = s;
    return s;
}

// ---- answers ----
void net_http_add_header(NetHttpRequest *r, const char *name, const char *value)
{
    Request *q = r->own;
    net_buf_printf(&q->extra, "%s: %s\r\n", name, value);
}

void net_http_reply(NetHttpRequest *r, int status, const char *type, const void *data, size_t size)
{
    Request *q = r->own;
    if (q->replied)
        return;
    q->replied = 1;
    NetBuf head = { 0, 0, 0 };
    net_buf_printf(&head, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n",
                   status, reason(status), type ? type : "application/octet-stream", size);
    if (q->extra.size)
        net_buf_add(&head, q->extra.data, q->extra.size);
    net_buf_add(&head, "\r\n", 2);
    if (head.data && net_send(q->conn, head.data, (int)head.size) > 0 && size && strcmp(r->method, "HEAD") != 0)
        net_send(q->conn, data, (int)size);
    free(head.data);
}

void net_http_reply_text(NetHttpRequest *r, int status, const char *text)
{
    net_http_reply(r, status, "text/plain; charset=utf-8", text, strlen(text));
}

void net_http_reply_html(NetHttpRequest *r, int status, const char *html)
{
    net_http_reply(r, status, "text/html; charset=utf-8", html, strlen(html));
}

static const char *type_of(const char *path)
{
    static const char *types[][2] = {
        { "html", "text/html; charset=utf-8" }, { "htm", "text/html; charset=utf-8" },
        { "css", "text/css; charset=utf-8" }, { "js", "text/javascript; charset=utf-8" },
        { "json", "application/json" }, { "txt", "text/plain; charset=utf-8" }, { "xml", "application/xml" },
        { "png", "image/png" }, { "jpg", "image/jpeg" }, { "jpeg", "image/jpeg" }, { "gif", "image/gif" },
        { "webp", "image/webp" }, { "svg", "image/svg+xml" }, { "ico", "image/x-icon" }, { "bmp", "image/bmp" },
        { "wav", "audio/wav" }, { "mp3", "audio/mpeg" }, { "ogg", "audio/ogg" }, { "mp4", "video/mp4" },
        { "pdf", "application/pdf" }, { "zip", "application/zip" }, { "wasm", "application/wasm" },
        { "woff", "font/woff" }, { "woff2", "font/woff2" }, { "ttf", "font/ttf" },
    };
    const char *dot = strrchr(path, '.');
    if (dot && !strpbrk(dot, "\\/"))
        for (size_t i = 0; i < sizeof types / sizeof types[0]; i++)
            if (net_same(dot + 1, types[i][0]))
                return types[i][1];
    return "application/octet-stream";
}

int net_http_reply_file(NetHttpRequest *r, const char *path)
{
    wchar_t *w = net_wide(path);
    HANDLE f = w ? CreateFileW(w, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)
                 : INVALID_HANDLE_VALUE;
    free(w);
    if (f == INVALID_HANDLE_VALUE) {
        net_set_system_error(path, GetLastError());
        return 0;
    }
    LARGE_INTEGER size;
    char *data = 0;
    DWORD got = 0;
    int ok = GetFileSizeEx(f, &size) && size.QuadPart < BODY_LIMIT * 16LL &&
             (data = malloc((size_t)size.QuadPart + 1)) != 0 &&
             ReadFile(f, data, (DWORD)size.QuadPart, &got, 0) && got == (DWORD)size.QuadPart;
    if (!ok)
        net_set_system_error(path, GetLastError());
    CloseHandle(f);
    if (ok)
        net_http_reply(r, 200, type_of(path), data, (size_t)got);
    free(data);
    return ok;
}

// ---- what the request says ----
const char *net_http_header(NetHttpRequest *r, const char *name)
{
    return keep(r->own, net_find_header(r->headers, name));
}

static const char *param_in(Request *q, const char *list, const char *name)
{
    for (const char *p = list; p && *p;) {
        const char *amp = strchr(p, '&'), *end = amp ? amp : p + strlen(p);
        const char *eq = memchr(p, '=', (size_t)(end - p));
        char *key = net_strndup(p, (size_t)((eq ? eq : end) - p)), *dkey = key ? net_url_decode(key) : 0;
        int same = dkey && strcmp(dkey, name) == 0;
        free(key);
        free(dkey);
        if (same) {
            char *value = eq ? net_strndup(eq + 1, (size_t)(end - eq - 1)) : net_strdup("");
            char *dvalue = value ? net_url_decode(value) : 0;
            free(value);
            return keep(q, dvalue);
        }
        p = amp ? amp + 1 : end;
    }
    return 0;
}

const char *net_http_param(NetHttpRequest *r, const char *name)
{
    const char *v = param_in(r->own, r->query, name);
    if (v)
        return v;
    char *type = net_find_header(r->headers, "Content-Type");
    int form = type && strncmp(type, "application/x-www-form-urlencoded", 33) == 0;
    free(type);
    return form ? param_in(r->own, r->body, name) : 0;
}

NetHttpServer *net_http_server(NetHttpRequest *r)
{
    return ((Request *)r->own)->server;
}

// ---- one connection ----
static void serve(NetHttpServer *sv, NetSocket *conn)
{
    net_socket_timeout(conn, 10000);                // a client that says nothing does not hold the server
    NetBuf in = { 0, 0, 0 };
    char *end = 0;
    char chunk[8192];
    for (;;) {
        int n = net_recv(conn, chunk, sizeof chunk);
        if (n <= 0 || !net_buf_add(&in, chunk, (size_t)n)) {
            free(in.data);
            return;
        }
        end = strstr(in.data, "\r\n\r\n");
        if ((end && (size_t)(end - in.data) > HEAD_LIMIT) || (!end && in.size > HEAD_LIMIT)) {
            net_send_text(conn, "HTTP/1.1 431 Request Header Fields Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            free(in.data);
            return;
        }
        if (end)
            break;
    }
    size_t head_size = (size_t)(end - in.data) + 4;

    // "GET /path?query HTTP/1.1"
    char *line_end = strstr(in.data, "\r\n");
    *line_end = 0;
    char *method = in.data, *target = strchr(method, ' ');
    char *version = target ? strchr(target + 1, ' ') : 0;
    if (!target || !version) {
        net_send_text(conn, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        free(in.data);
        return;
    }
    *target++ = 0;
    *version = 0;
    char *query = strchr(target, '?');
    if (query)
        *query++ = 0;
    char *hash = query ? strchr(query, '#') : 0;
    if (hash)
        *hash = 0;
    in.data[head_size - 2] = 0;                     // the headers end with their last "\r\n"
    char *headers = line_end + 2;

    // the body: Content-Length bytes after the head
    size_t want = 0;
    char *length = net_find_header(headers, "Content-Length");
    if (length)
        want = (size_t)strtoull(length, 0, 10);
    free(length);
    if (want > BODY_LIMIT) {
        net_send_text(conn, "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        free(in.data);
        return;
    }
    NetBuf body = { 0, 0, 0 };
    net_buf_add(&body, in.data + head_size, in.size - head_size);
    char *coding = net_find_header(headers, "Transfer-Encoding");
    int chunked = coding && strstr(coding, "chunked");
    free(coding);
    if (chunked) {                                  // the body in pieces: "size in hex\r\n bytes \r\n" ... "0\r\n\r\n"
        NetBuf raw = body, out = { 0, 0, 0 };
        size_t at = 0;
        int ok = net_buf_add(&out, "", 0);
        while (ok) {
            char *eol = raw.data ? strstr(raw.data + at, "\r\n") : 0;
            if (!eol) {
                int n = net_recv(conn, chunk, sizeof chunk);
                ok = n > 0 && raw.size < BODY_LIMIT && net_buf_add(&raw, chunk, (size_t)n);
                continue;
            }
            size_t piece = (size_t)strtoull(raw.data + at, 0, 16), from = (size_t)(eol - raw.data) + 2;
            if (piece == 0)
                break;                              // the trailing headers, if any, are not needed
            if (out.size + piece > BODY_LIMIT) {
                ok = 0;
                break;
            }
            while (ok && raw.size < from + piece + 2) {
                int n = net_recv(conn, chunk, sizeof chunk);
                ok = n > 0 && net_buf_add(&raw, chunk, (size_t)n);
            }
            if (ok)
                ok = net_buf_add(&out, raw.data + from, piece);
            at = from + piece + 2;
        }
        free(raw.data);
        if (!ok) {
            net_send_text(conn, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            free(out.data);
            free(in.data);
            return;
        }
        body = out;
        want = body.size;
    }
    while (body.data && body.size < want) {
        int n = net_recv(conn, chunk, sizeof chunk);
        if (n <= 0 || !net_buf_add(&body, chunk, (size_t)n)) {
            free(body.data);
            free(in.data);
            return;
        }
    }
    if (!body.data) {
        free(in.data);
        return;
    }
    if (body.size > want) {                         // more than it said: the rest is not ours
        body.size = want;
        body.data[want] = 0;
    }

    // the path as it was written, without %XX ("+" stays "+" in a path)
    char *path = net_strdup(target), *o = path;
    for (const char *p = target; path && *p; p++) {
        int h1 = p[1], h2 = h1 ? p[2] : 0;
        if (*p == '%' && isxdigit(h1) && isxdigit(h2)) {
            *o++ = (char)((isdigit(h1) ? h1 - '0' : (h1 | 32) - 'a' + 10) * 16 + (isdigit(h2) ? h2 - '0' : (h2 | 32) - 'a' + 10));
            p += 2;
        } else
            *o++ = *p;
    }
    if (path)
        *o = 0;
    // "/files/../../secret": a path that climbs out is refused before any handler sees it
    const char *whole = path ? path : target;
    for (const char *p = whole; *p; p++)
        if (p[0] == '.' && p[1] == '.' && (p == whole || p[-1] == '/' || p[-1] == '\\') &&
            (p[2] == 0 || p[2] == '/' || p[2] == '\\')) {
            net_send_text(conn, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            free(path);
            free(body.data);
            free(in.data);
            return;
        }

    Request q;
    memset(&q, 0, sizeof q);
    q.server = sv;
    q.conn = conn;
    NetHttpRequest r;
    r.method = method;
    r.path = path ? path : target;
    r.query = query ? query : "";
    r.headers = headers;
    r.body = body.data;
    r.size = body.size;
    r.peer = net_peer(conn);
    r.own = &q;
    EnterCriticalSection(&sv->handler_lock);
    EnterCriticalSection(&sv->conns_lock);
    sv->answering = conn;
    LeaveCriticalSection(&sv->conns_lock);
    g_in_handler = sv;
    sv->handler(&r, sv->user);
    if (!q.replied)
        net_http_reply_text(&r, 404, "Не найдено");
    g_in_handler = 0;
    EnterCriticalSection(&sv->conns_lock);
    sv->answering = 0;
    LeaveCriticalSection(&sv->conns_lock);
    LeaveCriticalSection(&sv->handler_lock);

    for (int i = 0; i < q.count; i++)
        free(q.strings[i]);
    free(q.strings);
    free(q.extra.data);
    free(path);
    free(body.data);
    free(in.data);
}

typedef struct { NetHttpServer *sv; NetSocket *conn; int slot; } Conn;

static DWORD WINAPI conn_thread(LPVOID arg)
{
    Conn c = *(Conn *)arg;
    free(arg);
    serve(c.sv, c.conn);
    EnterCriticalSection(&c.sv->conns_lock);
    c.sv->conns[c.slot] = 0;
    LeaveCriticalSection(&c.sv->conns_lock);
    net_close(c.conn);
    InterlockedDecrement(&c.sv->active);
    return 0;
}

// a connection into a free slot and a thread of its own (no thread: served here)
static void serve_one(NetHttpServer *sv, NetSocket *conn)
{
    while (sv->active >= CONN_MAX && !sv->stopping)
        Sleep(1);
    Conn *c = malloc(sizeof *c);
    int slot = -1;
    EnterCriticalSection(&sv->conns_lock);
    for (int i = 0; i < CONN_MAX && slot < 0; i++)
        if (!sv->conns[i]) {
            sv->conns[i] = conn;
            slot = i;
        }
    LeaveCriticalSection(&sv->conns_lock);
    if (c && slot >= 0) {
        c->sv = sv;
        c->conn = conn;
        c->slot = slot;
        InterlockedIncrement(&sv->active);
        HANDLE t = CreateThread(0, 0, conn_thread, c, 0, 0);
        if (t) {
            CloseHandle(t);
            return;
        }
        InterlockedDecrement(&sv->active);
    }
    free(c);
    if (slot >= 0) {
        EnterCriticalSection(&sv->conns_lock);
        sv->conns[slot] = 0;
        LeaveCriticalSection(&sv->conns_lock);
    }
    serve(sv, conn);
    net_close(conn);
}

static void serve_all(NetHttpServer *sv)
{
    while (!sv->stopping) {
        NetSocket *conn = net_accept(sv->listen);
        if (!conn) {
            if (sv->stopping)
                break;
            Sleep(10);                              // a failed accept: do not spin
            continue;
        }
        serve_one(sv, conn);
    }
    // stopped (from outside or by its own handler): the connections still read or waiting
    // for the handler are cut, the one being answered finishes; then all are waited for
    EnterCriticalSection(&sv->conns_lock);
    for (int i = 0; i < CONN_MAX; i++)
        if (sv->conns[i] && sv->conns[i] != sv->answering) {
            SOCKET k = sv->conns[i]->s;             // closed here: its recv returns at once
            sv->conns[i]->s = INVALID_SOCKET;       // (net_close of its thread closes nothing then)
            closesocket(k);
        }
    LeaveCriticalSection(&sv->conns_lock);
    while (sv->active > 0)
        Sleep(5);
}

static void server_free(NetHttpServer *sv)
{
    DeleteCriticalSection(&sv->handler_lock);
    DeleteCriticalSection(&sv->conns_lock);
    free(sv);
}

static NetHttpServer *server_new(int port, NetHttpHandler handler, void *user, int local)
{
    NetHttpServer *sv = calloc(1, sizeof *sv);
    if (!sv) {
        net_set_error("Не хватило памяти");
        return 0;
    }
    sv->listen = local ? net_listen_local(port) : net_listen(port);
    if (!sv->listen) {
        free(sv);
        return 0;
    }
    sv->handler = handler;
    sv->user = user;
    InitializeCriticalSection(&sv->handler_lock);
    InitializeCriticalSection(&sv->conns_lock);
    return sv;
}

static DWORD WINAPI server_thread(LPVOID arg)
{
    NetHttpServer *sv = arg;
    serve_all(sv);
    free(sv->listen);                               // its socket was closed by net_http_stop
    sv->listen = 0;
    return 0;                                       // sv itself: net_http_stop frees it
}

static NetHttpServer *start(int port, NetHttpHandler handler, void *user, int local)
{
    NetHttpServer *sv = server_new(port, handler, user, local);
    if (!sv)
        return 0;
    sv->thread = CreateThread(0, 0, server_thread, sv, CREATE_SUSPENDED, &sv->thread_id);
    if (!sv->thread) {
        net_set_system_error("Поток сервера не создан", GetLastError());
        net_close(sv->listen);
        server_free(sv);
        return 0;
    }
    ResumeThread(sv->thread);
    return sv;
}

int net_http_run(int port, NetHttpHandler handler, void *user)
{
    NetHttpServer *sv = server_new(port, handler, user, 0);
    if (!sv)
        return -1;
    sv->thread_id = GetCurrentThreadId();
    serve_all(sv);
    free(sv->listen);
    server_free(sv);
    return 0;
}

int net_http_port(NetHttpServer *sv)
{
    return net_local_port(sv->listen);
}

NetHttpServer *net_http_start(int port, NetHttpHandler handler, void *user) { return start(port, handler, user, 0); }
NetHttpServer *net_http_start_local(int port, NetHttpHandler handler, void *user) { return start(port, handler, user, 1); }

// Stopped from outside: it waits for the server's thread and frees all. Stopped by its own
// handler: the server ends after that request; a net_http_stop called later (outside) frees it.
void net_http_stop(NetHttpServer *sv)
{
    if (!sv)
        return;
    int inside = g_in_handler == sv;                // called by its own handler
    HANDLE thread = sv->thread;                     // after closesocket sv may be gone (net_http_run frees it)
    if (sv->stopping) {
        if (sv->stopping == 2 && thread && !inside) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
            server_free(sv);
        }
        return;
    }
    sv->stopping = inside ? 2 : 1;
    closesocket(sv->listen->s);                     // accept returns at once
    if (thread && !inside) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        server_free(sv);
    }
}
