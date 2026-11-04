// HTTP and HTTPS through WinHTTP: one session for the whole program (the proxy of the
// system, gzip unpacked, redirects followed), a request = connect, send, read all.
#include "net_int.h"

typedef struct {
    char **names;                                   // the strings net_header gave out
    int count;
} Own;

static SRWLOCK g_lock = SRWLOCK_INIT;
static HINTERNET g_session;
static wchar_t *g_agent;

void net_set_agent(const char *agent)
{
    AcquireSRWLockExclusive(&g_lock);
    free(g_agent);
    g_agent = agent && *agent ? net_wide(agent) : 0;      // 0 or "": the name WinCC gives
    if (g_session) {
        const wchar_t *name = g_agent ? g_agent : L"WinCC net";
        WinHttpSetOption(g_session, WINHTTP_OPTION_USER_AGENT, (LPVOID)name, (DWORD)wcslen(name));
    }
    ReleaseSRWLockExclusive(&g_lock);
}

static HINTERNET session(void)
{
    AcquireSRWLockExclusive(&g_lock);
    if (!g_session) {
        const wchar_t *agent = g_agent ? g_agent : L"WinCC net";
        // the proxy as Windows has it set; before Windows 8.1 -- as the registry says
        g_session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, 0, 0, 0);
        if (!g_session)
            g_session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
        if (g_session) {
            DWORD v = WINHTTP_DECOMPRESSION_FLAG_ALL;
            WinHttpSetOption(g_session, WINHTTP_OPTION_DECOMPRESSION, &v, sizeof v);
            v = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            if (!WinHttpSetOption(g_session, WINHTTP_OPTION_SECURE_PROTOCOLS, &v, sizeof v)) {
                v = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;    // a Windows without TLS 1.3
                WinHttpSetOption(g_session, WINHTTP_OPTION_SECURE_PROTOCOLS, &v, sizeof v);
            }
        }
    }
    HINTERNET s = g_session;
    ReleaseSRWLockExclusive(&g_lock);
    if (!s)
        net_set_system_error("WinHTTP не открылся", GetLastError());
    return s;
}

static NetResponse *response_new(void)
{
    NetResponse *r = calloc(1, sizeof *r);
    Own *own = calloc(1, sizeof *own);
    if (!r || !own) {
        free(r);
        free(own);
        net_set_error("Не хватило памяти");
        return 0;
    }
    r->own = own;
    return r;
}

static NetResponse *response_fail(NetResponse *r)
{
    r->error = net_strdup(net_error());
    if (!r->error)
        r->error = "Не хватило памяти";
    r->status = 0;
    return r;
}

void net_free(NetResponse *r)
{
    if (!r)
        return;
    Own *own = r->own;
    if (own) {
        for (int i = 0; i < own->count; i++)
            free(own->names[i]);
        free(own->names);
        free(own);
    }
    free(r->body);
    free(r->headers);
    if (r->error && strcmp(r->error, "Не хватило памяти") != 0)
        free((char *)r->error);
    free(r);
}

const char *net_header(NetResponse *r, const char *name)
{
    if (!r || !r->headers)
        return 0;
    char *value = net_find_header(r->headers, name);
    Own *own = r->own;
    char **names = value ? realloc(own->names, sizeof(char *) * (size_t)(own->count + 1)) : 0;
    if (!names) {
        free(value);
        return 0;
    }
    own->names = names;
    own->names[own->count++] = value;
    return value;
}

// the request itself: into the response, or (file != INVALID_HANDLE_VALUE) into a file
static int run(NetResponse *r, const char *method, const char *url, const char *headers, const void *data, size_t size,
               HANDLE file)
{
    HINTERNET s = session();
    if (!s)
        return 0;
    wchar_t *wurl = net_wide(url);
    if (!wurl) {
        net_set_error("Не хватило памяти");
        return 0;
    }
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        net_set_errorf("Адрес не понят: %s", url);
        free(wurl);
        return 0;
    }
    wchar_t host[256];
    if (uc.dwHostNameLength >= 256) {
        net_set_errorf("Слишком длинное имя сервера: %s", url);
        free(wurl);
        return 0;
    }
    memcpy(host, uc.lpszHostName, sizeof(wchar_t) * uc.dwHostNameLength);
    host[uc.dwHostNameLength] = 0;
    // the path with "?..." after it: both lie together in wurl; "#..." is not for the server
    wchar_t *path = uc.dwUrlPathLength ? uc.lpszUrlPath : uc.dwExtraInfoLength ? uc.lpszExtraInfo : 0;
    wchar_t *hash = path ? wcschr(path, L'#') : 0;
    if (hash)
        *hash = 0;
    if (path && !path[0])
        path = 0;
    int ok = 0;
    HINTERNET c = 0, q = 0;
    wchar_t *wmethod = net_wide(method), *wheaders = net_wide(headers);
    int t = net_timeout_ms();
    c = WinHttpConnect(s, host, uc.nPort, 0);
    if (!c) {
        net_set_system_error("Нет соединения", GetLastError());
        goto done;
    }
    q = WinHttpOpenRequest(c, wmethod, path ? path : L"/", 0,
                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                           uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!q) {
        net_set_system_error("Запрос не создан", GetLastError());
        goto done;
    }
    WinHttpSetTimeouts(q, t, t, t, t);
    if (!WinHttpSendRequest(q, wheaders ? wheaders : WINHTTP_NO_ADDITIONAL_HEADERS, wheaders ? (DWORD)-1 : 0,
                            (LPVOID)data, (DWORD)size, (DWORD)size, 0) ||
        !WinHttpReceiveResponse(q, 0)) {
        net_set_system_error(url, GetLastError());
        goto done;
    }

    DWORD status = 0, len = sizeof status;
    WinHttpQueryHeaders(q, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &len, WINHTTP_NO_HEADER_INDEX);
    r->status = (int)status;
    len = 0;
    WinHttpQueryHeaders(q, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, 0, &len,
                        WINHTTP_NO_HEADER_INDEX);
    wchar_t *raw = len ? malloc(len + sizeof(wchar_t)) : 0;
    if (raw && WinHttpQueryHeaders(q, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw, &len,
                                   WINHTTP_NO_HEADER_INDEX)) {
        raw[len / sizeof(wchar_t)] = 0;
        // the first line is "HTTP/1.1 200 OK": the headers are what follows it
        wchar_t *first = wcsstr(raw, L"\r\n");
        r->headers = net_utf8(first ? first + 2 : raw, -1);
    }
    free(raw);
    if (!r->headers)
        r->headers = net_strdup("");

    size_t total = 0;
    wchar_t number[32];
    len = sizeof number;
    if (WinHttpQueryHeaders(q, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, number, &len,
                            WINHTTP_NO_HEADER_INDEX))
        total = (size_t)wcstoull(number, 0, 10);
    void *puser;
    NetProgress progress = net_progress_fn(&puser);
    NetBuf body = { 0, 0, 0 };
    net_buf_add(&body, "", 0);
    size_t done = 0;
    char chunk[65536];
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(q, chunk, sizeof chunk, &got)) {
            net_set_system_error("Ответ оборвался", GetLastError());
            free(body.data);
            goto done;
        }
        if (!got)
            break;
        done += got;
        if (file != INVALID_HANDLE_VALUE) {
            DWORD written;
            if (!WriteFile(file, chunk, got, &written, 0) || written != got) {
                net_set_system_error("Файл не пишется", GetLastError());
                free(body.data);
                goto done;
            }
        } else if (!net_buf_add(&body, chunk, got)) {
            net_set_error("Не хватило памяти");
            free(body.data);
            goto done;
        }
        if (progress)
            progress(done, total, puser);
    }
    r->body = body.data;
    r->size = body.size;
    if (!r->body) {
        net_set_error("Не хватило памяти");
        goto done;
    }
    net_set_error("");
    ok = 1;
done:
    if (q) WinHttpCloseHandle(q);
    if (c) WinHttpCloseHandle(c);
    free(wurl);
    free(wmethod);
    free(wheaders);
    return ok;
}

NetResponse *net_request(const char *method, const char *url, const char *headers, const void *data, size_t size)
{
    NetResponse *r = response_new();
    if (!r)
        return 0;
    if (!run(r, method ? method : "GET", url, headers, data, size, INVALID_HANDLE_VALUE))
        return response_fail(r);
    return r;
}

NetResponse *net_get(const char *url)
{
    return net_request("GET", url, 0, 0, 0);
}

NetResponse *net_post(const char *url, const char *type, const void *data, size_t size)
{
    char head[300];
    snprintf(head, sizeof head, "Content-Type: %s\r\n", type ? type : "application/octet-stream");
    return net_request("POST", url, head, data, size);
}

NetResponse *net_post_text(const char *url, const char *type, const char *text)
{
    return net_post(url, type ? type : "text/plain; charset=utf-8", text, strlen(text));
}

int net_download(const char *url, const char *path)
{
    // into "имя.part" first: the file of that name (an older copy) stays until the new one is whole
    wchar_t *wpath = net_wide(path), *wpart = net_wide(path);
    size_t n = wpart ? wcslen(wpart) : 0;
    wchar_t *grown = wpart ? realloc(wpart, sizeof(wchar_t) * (n + 6)) : 0;
    if (!wpath || !grown) {
        free(wpath);
        free(grown ? grown : wpart);
        net_set_error("Не хватило памяти");
        return 0;
    }
    wpart = grown;
    wcscpy(wpart + n, L".part");
    HANDLE f = CreateFileW(wpart, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE) {
        net_set_system_error(path, GetLastError());
        free(wpath);
        free(wpart);
        return 0;
    }
    NetResponse *r = response_new();
    int ok = r && run(r, "GET", url, 0, 0, 0, f);
    if (ok && (r->status < 200 || r->status > 299)) {
        net_set_errorf("Сервер ответил %d: %s", r->status, url);
        ok = 0;
    }
    CloseHandle(f);
    if (ok && !MoveFileExW(wpart, wpath, MOVEFILE_REPLACE_EXISTING)) {
        net_set_system_error(path, GetLastError());
        ok = 0;
    }
    if (!ok)
        DeleteFileW(wpart);
    free(wpath);
    free(wpart);
    net_free(r);
    return ok;
}
