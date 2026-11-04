// server: маленький сайт на этом компьютере — страница с формой, время, остановка.
// Запустите и откройте в браузере http://localhost:8080
#include <net.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *page =
    "<!doctype html><meta charset=utf-8><title>WinCC</title>"
    "<style>body{font:18px Segoe UI;margin:40px;max-width:600px}input,button{font:inherit;padding:6px}</style>"
    "<h1>Сайт на Си</h1>"
    "<form action=/hello method=post><input name=name placeholder='Как вас зовут?'> <button>Поздороваться</button></form>"
    "<p><a href=/time>Который час?</a> · <a href=/stop>Остановить сервер</a>";

static void site(NetHttpRequest *r, void *user)
{
    (void)user;
    printf("%s %s %s\n", r->peer, r->method, r->path);
    if (strcmp(r->path, "/") == 0)
        net_http_reply_html(r, 200, page);
    else if (strcmp(r->path, "/hello") == 0) {
        const char *name = net_http_param(r, "name");
        char html[600];
        snprintf(html, sizeof html, "<!doctype html><meta charset=utf-8><h1>Привет, %s!</h1><a href=/>Назад</a>",
                 name && name[0] ? name : "незнакомец");     // в настоящем сайте имя надо очистить от < >
        net_http_reply_html(r, 200, html);
    } else if (strcmp(r->path, "/time") == 0) {
        time_t t = time(0);
        char text[64];
        strftime(text, sizeof text, "%d.%m.%Y %H:%M:%S", localtime(&t));
        net_http_reply_text(r, 200, text);
    } else if (strcmp(r->path, "/stop") == 0) {
        net_http_reply_text(r, 200, "Сервер остановлен.");
        net_http_stop(net_http_server(r));
    }
}

int main(void)
{
    printf("Откройте http://localhost:8080  (остановить: /stop)\n");
    if (net_http_run(8080, site, 0) < 0) {
        printf("%s\n", net_error());
        return 1;
    }
    printf("Остановлен.\n");
    return 0;
}
