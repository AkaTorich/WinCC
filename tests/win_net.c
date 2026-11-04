// net.h without the internet: the program is its own web server (net_http_start) and asks
// it over HTTP; a TCP echo in a thread; UDP packets between two sockets; names, %XX, errors.
#include "test.h"
#ifndef _WIN32                          // DynastyOS (cctest): no Windows there
int main(void) { return 0; }
#else
#include <net.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void handler(NetHttpRequest *r, void *user)
{
    (*(int *)user)++;
    if (strcmp(r->path, "/hello") == 0) {
        const char *name = net_http_param(r, "name");
        char text[200];
        snprintf(text, sizeof text, "Привет, %s", name ? name : "?");
        net_http_add_header(r, "X-Reply", "да");
        net_http_reply_text(r, 200, text);
    } else if (strcmp(r->path, "/echo") == 0) {
        const char *type = net_http_header(r, "content-type");
        net_http_reply(r, 201, type, r->body, r->size);
    } else if (strcmp(r->path, "/form") == 0) {
        const char *a = net_http_param(r, "a"), *b = net_http_param(r, "b");
        char text[200];
        snprintf(text, sizeof text, "%s|%s|%s", r->method, a ? a : "-", b ? b : "-");
        net_http_reply_text(r, 200, text);
    } else if (strcmp(r->path, "/путь до/x") == 0) {
        net_http_reply_text(r, 200, r->query);
    } else if (strcmp(r->path, "/stop") == 0) {      // the server stops itself after this answer
        net_http_reply_text(r, 200, "стоп");
        net_http_stop(net_http_server(r));
    } else if (strcmp(r->path, "/big") == 0) {
        char *big = malloc(300000);
        for (int i = 0; i < 300000; i++)
            big[i] = (char)(i * 7);
        net_http_reply(r, 200, 0, big, 300000);
        free(big);
    }
}

static DWORD WINAPI echo_thread(LPVOID arg)
{
    NetSocket *server = arg;
    NetSocket *c = net_accept(server);
    if (!c)
        return 1;
    char *line;
    while ((line = net_recv_line(c)) != 0) {
        net_printf(c, "эхо: %s\n", line);
        free(line);
    }
    net_close(c);
    return 0;
}

static size_t g_done, g_total;
static void progress(size_t done, size_t total, void *user)
{
    g_done = done;
    g_total = total;
    (*(int *)user)++;
}

int main(void)
{
    // %XX
    char *e = net_url_encode("а б&в=1~");
    ASSERT(0, strcmp(e, "%D0%B0+%D0%B1%26%D0%B2%3D1~"));
    char *d = net_url_decode(e);
    ASSERT(0, strcmp(d, "а б&в=1~"));
    free(e);
    free(d);

    // names
    char *ip = net_resolve("localhost");
    ASSERT(1, ip && (strcmp(ip, "127.0.0.1") == 0 || strcmp(ip, "::1") == 0));
    free(ip);
    char *host = net_hostname();
    ASSERT(1, host && host[0] != 0);
    free(host);

    // the web server and requests to it
    int calls = 0;
    NetHttpServer *sv = net_http_start(0, handler, &calls);
    ASSERT(1, sv != 0);
    int port = net_http_port(sv);
    ASSERT(1, port > 0);
    char url[200];

    snprintf(url, sizeof url, "http://127.0.0.1:%d/hello?name=%%D0%%9C%%D0%%B8%%D1%%80", port);
    NetResponse *r = net_get(url);
    ASSERT(0, r->error != 0);
    ASSERT(200, r->status);
    ASSERT(0, strcmp(r->body, "Привет, Мир"));
    ASSERT(20, (long long)r->size);
    const char *xr = net_header(r, "x-reply");
    ASSERT(1, xr && strcmp(xr, "да") == 0);
    const char *ct = net_header(r, "Content-Type");
    ASSERT(1, ct && strcmp(ct, "text/plain; charset=utf-8") == 0);
    net_free(r);

    char bin[1000];
    for (int i = 0; i < 1000; i++)
        bin[i] = (char)(i % 256);                   // zeros inside
    snprintf(url, sizeof url, "http://localhost:%d/echo", port);
    r = net_post(url, "application/x-test", bin, sizeof bin);
    ASSERT(201, r->status);
    ASSERT(1000, (long long)r->size);
    ASSERT(0, memcmp(r->body, bin, sizeof bin));
    ct = net_header(r, "content-type");
    ASSERT(1, ct && strcmp(ct, "application/x-test") == 0);
    net_free(r);

    snprintf(url, sizeof url, "http://127.0.0.1:%d/form?a=%%D1%%8F", port);
    r = net_post_text(url, "application/x-www-form-urlencoded", "b=%D0%BC%D1%8B+%D0%B2%D1%81%D0%B5&c=1");
    ASSERT(0, strcmp(r->body, "POST|я|мы все"));
    net_free(r);

    snprintf(url, sizeof url, "http://127.0.0.1:%d/%%D0%%BF%%D1%%83%%D1%%82%%D1%%8C%%20%%D0%%B4%%D0%%BE/x?q=1#frag", port);
    r = net_request("GET", url, "X-Test: 1\r\n", 0, 0);
    ASSERT(200, r->status);
    ASSERT(0, strcmp(r->body, "q=1"));
    net_free(r);

    snprintf(url, sizeof url, "http://127.0.0.1:%d/nothing", port);
    r = net_get(url);
    ASSERT(404, r->status);
    ASSERT(0, strcmp(r->body, "Не найдено"));
    net_free(r);

    // a file, with progress
    int steps = 0;
    net_on_progress(progress, &steps);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/big", port);
    ASSERT(1, net_download(url, "скачано.bin"));
    net_on_progress(0, 0);
    ASSERT(1, steps > 0);
    ASSERT(300000, (long long)g_done);
    ASSERT(300000, (long long)g_total);
    FILE *f = fopen("скачано.bin", "rb");
    ASSERT(1, f != 0);
    if (f) {
        fseek(f, 0, SEEK_END);
        ASSERT(300000, ftell(f));
        fseek(f, 1000, SEEK_SET);
        ASSERT((char)7000, (char)fgetc(f));
        fclose(f);
    }
    remove("скачано.bin");
    snprintf(url, sizeof url, "http://127.0.0.1:%d/nothing", port);
    ASSERT(0, net_download(url, "нет.bin"));
    ASSERT(1, strstr(net_error(), "404") != 0);
    ASSERT(0, fopen("нет.bin", "rb") != 0);

    ASSERT(7, calls);
    net_http_stop(sv);

    // no server any more: an error, not a crash
    net_set_timeout(3000);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/hello", port);
    r = net_get(url);
    ASSERT(1, r->error != 0 && r->error[0] != 0);
    ASSERT(0, r->status);
    net_free(r);
    r = net_get("это не адрес");
    ASSERT(1, r->error != 0);
    net_free(r);

    // TCP: an echo in another thread
    NetSocket *ls = net_listen(0);
    ASSERT(1, ls != 0);
    int tport = net_local_port(ls);
    HANDLE th = CreateThread(0, 0, echo_thread, ls, 0, 0);
    NetSocket *c = net_connect("localhost", tport);
    ASSERT(1, c != 0);
    if (c) {
        ASSERT(1, strstr(net_peer(c), ":") != 0);
        net_send_text(c, "раз\r\nдва\n");
        char *l1 = net_recv_line(c), *l2 = net_recv_line(c);
        ASSERT(0, strcmp(l1, "эхо: раз"));
        ASSERT(0, strcmp(l2, "эхо: два"));
        free(l1);
        free(l2);
        ASSERT(0, net_ready(c, 50));
        net_send_text(c, "три\n");
        ASSERT(1, net_ready(c, 3000));
        char buf[64];
        ASSERT(1, net_recv_all(c, buf, 15));        // "эхо: три\n" is 15 bytes in UTF-8
        ASSERT(0, memcmp(buf, "эхо: три\n", 15));
        net_close(c);
    }
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    net_close(ls);
    ASSERT(1, net_connect("127.0.0.1", tport) == 0);    // nobody listens now
    ASSERT(1, net_error()[0] != 0);

    // UDP
    NetSocket *u1 = net_udp(0), *u2 = net_udp(0);
    ASSERT(1, u1 && u2);
    net_socket_timeout(u2, 3000);
    ASSERT(6, net_udp_send(u1, "127.0.0.1", net_local_port(u2), "пак", 6));
    char got[32], from[80];
    ASSERT(6, net_udp_recv(u2, got, sizeof got, from, sizeof from));
    ASSERT(0, memcmp(got, "пак", 6));
    char want[80];
    snprintf(want, sizeof want, "127.0.0.1:%d", net_local_port(u1));
    ASSERT(0, strcmp(from, want));
    net_socket_timeout(u2, 200);
    ASSERT(-1, net_udp_recv(u2, got, sizeof got, 0, 0));
    net_close(u1);
    net_close(u2);

    // the same port twice
    NetSocket *p1 = net_listen(0);
    NetSocket *p2 = net_listen(net_local_port(p1));
    ASSERT(1, p2 == 0);
    ASSERT(1, strstr(net_error(), "занят") != 0);
    net_close(p1);

    // UDP: a packet to a port nobody listens on does not break the next receive (no 10054)
    NetSocket *a1 = net_udp(0), *a2 = net_udp(0);
    net_socket_timeout(a1, 3000);
    net_udp_send(a1, "127.0.0.1", 9, "x", 1);
    net_udp_send(a2, "127.0.0.1", net_local_port(a1), "ok", 2);
    ASSERT(2, net_udp_recv(a1, got, sizeof got, 0, 0));
    ASSERT(0, net_error()[0]);                   // a success clears the error
    net_close(a1);
    net_close(a2);

    // the server again, only for this computer: chunked bodies, ".." refused, an old file kept
    calls = 0;
    sv = net_http_start_local(0, handler, &calls);
    ASSERT(1, sv != 0);
    port = net_http_port(sv);
    c = net_connect("127.0.0.1", port);
    ASSERT(1, c != 0);
    if (c) {
        net_printf(c, "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n"
                      "6\r\nпри\r\n6\r\nвет\r\n0\r\n\r\n");    // sizes in bytes: "при" is 6 in UTF-8
        char *status = net_recv_line(c);
        ASSERT(1, status && strstr(status, "201") != 0);
        free(status);
        char *line;
        while ((line = net_recv_line(c)) != 0 && line[0])
            free(line);
        free(line);
        char text[32] = "";
        ASSERT(1, net_recv_all(c, text, 12));
        ASSERT(0, memcmp(text, "привет", 12));
        net_close(c);
    }
    c = net_connect("127.0.0.1", port);
    if (c) {
        net_send_text(c, "GET /files/../../secret HTTP/1.1\r\nHost: x\r\n\r\n");
        char *status = net_recv_line(c);
        ASSERT(1, status && strstr(status, "400") != 0);
        free(status);
        net_close(c);
    }
    FILE *old = fopen("старое.bin", "wb");
    fputs("прежнее", old);
    fclose(old);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/nothing", port);
    ASSERT(0, net_download(url, "старое.bin"));
    old = fopen("старое.bin", "rb");
    ASSERT(1, old != 0);
    if (old) {
        char was[32] = "";
        fgets(was, sizeof was, old);
        fclose(old);
        ASSERT(0, strcmp(was, "прежнее"));
    }
    remove("старое.bin");
    ASSERT(0, fopen("старое.bin.part", "rb") != 0);
    ASSERT(1, calls == 2);                       // ".." never reached the handler
    // a client that sends half a request and waits keeps nobody else waiting
    NetSocket *slow = net_connect("127.0.0.1", port);
    if (slow)
        net_send_text(slow, "GET /hello HTTP/1.1\r\nHost: x\r\n");
    unsigned long long t0 = GetTickCount64();
    c = net_connect("127.0.0.1", port);
    if (c) {
        net_send_text(c, "GET /files/../x HTTP/1.1\r\nHost: x\r\n\r\n");
        char *status = net_recv_line(c);
        ASSERT(1, status && strstr(status, "400") != 0);
        free(status);
        net_close(c);
    }
    ASSERT(1, GetTickCount64() - t0 < 3000);   // not the slow one's 10 seconds
    net_http_stop(sv);                          // cuts the slow one: no waiting for it either
    ASSERT(1, GetTickCount64() - t0 < 3000);
    net_close(slow);
    // stopped by its own handler: the answer of /stop arrives whole, the slow one is cut at once
    sv = net_http_start_local(0, handler, &calls);
    ASSERT(1, sv != 0);
    port = net_http_port(sv);
    slow = net_connect("127.0.0.1", port);
    if (slow)
        net_send_text(slow, "GET /hello HTTP/1.1\r\nHost: x\r\n");
    Sleep(100);                                 // its thread is reading by now
    t0 = GetTickCount64();
    snprintf(url, sizeof url, "http://127.0.0.1:%d/stop", port);
    NetResponse *stopped = net_get(url);
    ASSERT(1, stopped->status == 200 && strcmp(stopped->body, "стоп") == 0);
    net_free(stopped);
    net_http_stop(sv);                          // from outside after that: waits for the server and frees it
    ASSERT(1, GetTickCount64() - t0 < 3000);
    net_close(slow);
    return g_fail;
}
#endif
