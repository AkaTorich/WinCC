// get: скачать страницу или файл.
//   get https://example.com            — код ответа, заголовки и начало страницы
//   get https://example.com/x.zip x.zip — сохранить в файл, с полосой хода
#include <net.h>
#include <stdio.h>
#include <string.h>

static void bar(size_t done, size_t total, void *user)
{
    (void)user;
    if (total) {
        int filled = (int)(done * 30 / total);
        printf("\r[%.*s%*s] %3d%%  %zu КБ", filled, "##############################", 30 - filled, "",
               (int)(done * 100 / total), done / 1024);
    } else
        printf("\r%zu КБ", done / 1024);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("get адрес [файл]\n");
        return 1;
    }
    if (argc > 2) {
        net_on_progress(bar, 0);
        int ok = net_download(argv[1], argv[2]);
        printf("\n%s\n", ok ? "Сохранено." : net_error());
        return ok ? 0 : 1;
    }
    NetResponse *r = net_get(argv[1]);
    if (r->error) {
        printf("Не вышло: %s\n", r->error);
        net_free(r);
        return 1;
    }
    printf("Ответ %d, %zu байт\n\n%s\n", r->status, r->size, r->headers);
    printf("%.600s%s\n", r->body, r->size > 600 ? "\n..." : "");
    net_free(r);
    return 0;
}
