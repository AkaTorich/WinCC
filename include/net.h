// net.h — сеть для программ на Си: скачать страницу или файл (HTTP и HTTPS через WinHTTP),
// отправить запрос серверу, соединиться напрямую (TCP), обменяться пакетами (UDP),
// поднять свой маленький веб-сервер. Библиотека — исходниками в lib\net, wcc подключает её сам.
//
//   #include <net.h>
//   #include <stdio.h>
//   int main(void) {
//       NetResponse *r = net_get("https://example.com");
//       if (r->error) printf("Не вышло: %s\n", r->error);
//       else printf("%d, %zu байт:\n%s\n", r->status, r->size, r->body);
//       net_free(r);
//   }
//
// Тексты и пути — UTF-8. Строки, которые функции отдают через malloc, освобождает free();
// ответы — net_free(). Что пошло не так, говорит net_error() (по-русски, словами Windows).
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <stddef.h>
#ifndef NET_INTERNAL
#pragma comment(lib, "net")
#endif

const char *net_error(void);                        // последняя ошибка этого потока; "" — не было

// ---- HTTP и HTTPS ----
typedef struct NetResponse {
    int status;             // 200, 404...; 0 — ответа нет вовсе (см. error)
    char *body;             // что пришло; после него всегда стоит 0, так что текст — готовая строка
    size_t size;            // сколько байт пришло
    char *headers;          // все заголовки ответа, строки "Имя: значение\r\n"
    const char *error;      // 0 — сервер ответил; иначе — почему нет
    void *own;              // служебное
} NetResponse;

NetResponse *net_get(const char *url);
NetResponse *net_post(const char *url, const char *type, const void *data, size_t size);
NetResponse *net_post_text(const char *url, const char *type, const char *text);
// метод любой ("PUT", "DELETE"...); headers — свои строки "Имя: значение\r\n" или 0
NetResponse *net_request(const char *method, const char *url, const char *headers, const void *data, size_t size);
const char *net_header(NetResponse *r, const char *name);   // значение заголовка ответа или 0
void net_free(NetResponse *r);

// скачать в файл: 1 — сохранён; 0 — нет (net_error() скажет почему). Качается в «имя.part»: прежний
// файл с тем же именем остаётся, пока новый не скачан целиком
int net_download(const char *url, const char *path);

typedef void (*NetProgress)(size_t done, size_t total, void *user);     // total = 0: размер неизвестен
void net_on_progress(NetProgress fn, void *user);   // для всех следующих запросов этого потока; 0 — убрать
void net_set_timeout(int ms);                       // на соединение и на каждый ответ; сначала 30 000
void net_set_agent(const char *agent);              // как программа называет себя серверу (латиницей); 0 — «WinCC net»

char *net_url_encode(const char *text);             // "а б&в" -> "%D0%B0+%D0%B1%26%D0%B2"; free()
char *net_url_decode(const char *text);             // обратно; free()

// ---- TCP: соединение двух программ ----
typedef struct NetSocket NetSocket;

NetSocket *net_connect(const char *host, int port); // к серверу; 0 — не вышло
NetSocket *net_listen(int port);                    // ждать соединений на порту (0 — любой свободный)
NetSocket *net_listen_local(int port);              // то же, но прийти можно только с этого компьютера
NetSocket *net_accept(NetSocket *server);           // ждёт, пока кто-то придёт
int net_send(NetSocket *s, const void *data, int size);     // отправляет всё; size или -1
int net_send_text(NetSocket *s, const char *text);
int net_printf(NetSocket *s, const char *format, ...);
int net_recv(NetSocket *s, void *buffer, int size); // >0 — сколько пришло; 0 — соединение закрыто; -1 — ошибка
int net_recv_all(NetSocket *s, void *buffer, int size);     // ровно size байт; 1 или 0
char *net_recv_line(NetSocket *s);                  // строка без "\r\n"; 0 — закрыто; free()
int net_ready(NetSocket *s, int ms);                // 1 — есть что читать (или закрыто), 0 — ждали ms зря
void net_socket_timeout(NetSocket *s, int ms);      // сколько ждать recv и send; 0 — без конца
int net_local_port(NetSocket *s);                   // свой порт (нужен после net_listen(0))
const char *net_peer(NetSocket *s);                 // кто на том конце: "192.168.1.5:50123"
SOCKET net_handle(NetSocket *s);                    // сам сокет — для функций winsock2.h
void net_close(NetSocket *s);

// ---- UDP: отдельные пакеты ----
NetSocket *net_udp(int port);                       // 0 — любой свободный порт; рассылка на всех включена
int net_udp_send(NetSocket *s, const char *host, int port, const void *data, int size);
// from получает "адрес:порт" отправителя (можно 0); -1 — ошибка или вышло время
int net_udp_recv(NetSocket *s, void *buffer, int size, char *from, int from_size);

// ---- имена ----
char *net_resolve(const char *host);                // первый адрес имени: "93.184.215.14"; free()
char *net_hostname(void);                           // имя этого компьютера; free()

// ---- свой веб-сервер ----
typedef struct NetHttpRequest {
    const char *method;     // "GET", "POST"...
    const char *path;       // "/hello" — без "?..."
    const char *query;      // что после "?", или ""
    const char *headers;    // все заголовки запроса
    const char *body;       // тело запроса (после него 0)
    size_t size;
    const char *peer;       // кто спросил
    void *own;              // служебное
} NetHttpRequest;
typedef struct NetHttpServer NetHttpServer;
typedef void (*NetHttpHandler)(NetHttpRequest *request, void *user);

// сервер работает в своём потоке: handler зовётся там же, по одному запросу за раз
NetHttpServer *net_http_start(int port, NetHttpHandler handler, void *user);     // 0 — порт занят
NetHttpServer *net_http_start_local(int port, NetHttpHandler handler, void *user);   // только для этого компьютера
int net_http_port(NetHttpServer *server);
void net_http_stop(NetHttpServer *server);          // ждёт конца; из своего handler — сервер кончит после ответа
int net_http_run(int port, NetHttpHandler handler, void *user);    // то же, но ждёт здесь, пока не остановят
NetHttpServer *net_http_server(NetHttpRequest *r);                 // чей запрос: чтобы остановить сервер из handler

const char *net_http_param(NetHttpRequest *r, const char *name);   // из "?a=1" и из формы; или 0
const char *net_http_header(NetHttpRequest *r, const char *name);  // заголовок запроса или 0
void net_http_reply(NetHttpRequest *r, int status, const char *type, const void *data, size_t size);
void net_http_reply_text(NetHttpRequest *r, int status, const char *text);  // text/plain, UTF-8
void net_http_reply_html(NetHttpRequest *r, int status, const char *html);
int net_http_reply_file(NetHttpRequest *r, const char *path);      // тип — по расширению; 0 — нет файла
void net_http_add_header(NetHttpRequest *r, const char *name, const char *value);   // до reply
// handler, не ответивший ничего, отвечает 404
