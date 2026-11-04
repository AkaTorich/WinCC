// errno.h: every code has its strerror text; the library sets EISDIR,
// ENOTDIR, EMFILE, ENOSYS where it can tell (the test host's files are flat:
// "errdir/a.txt" makes "errdir" a folder, as the system's folders are)
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

static int open_errno(const char *name, const char *mode)
{
    errno = 0;
    FILE *f = fopen(name, mode);
    if (f) { fclose(f); return -1; }
    return errno;
}

int main(void)
{
    // strerror knows every code of errno.h
    int codes[] = { EPERM, ENOENT, EIO, EBADF, ENOMEM, EACCES, EEXIST, ENOTDIR, EISDIR,
                    EINVAL, EMFILE, ENOSPC, ESPIPE, EDOM, ERANGE, ENOSYS, EILSEQ };
    for (int i = 0; i < (int)(sizeof(codes) / sizeof(codes[0])); i++)
        if (!strcmp(strerror(codes[i]), "Unknown error")) {
            printf("\n    код %d без описания", codes[i]);
            g_fail++;
        }
    ASSERT(0, strcmp(strerror(ENOTDIR), "Not a directory"));
    ASSERT(0, strcmp(strerror(EISDIR), "Is a directory"));
    ASSERT(0, strcmp(strerror(ENOSYS), "Function not implemented"));
    ASSERT(0, strcmp(strerror(12345), "Unknown error"));

    // a folder: "errdir/a.txt" is there
#ifdef _WIN32                           // Windows: a folder is made, not implied by its files
    system("mkdir errdir 2>nul");
#endif
    FILE *f = fopen("errdir/a.txt", "w");
    ASSERT(1, f != 0);
    fputs("a", f);
    fclose(f);
    ASSERT(EISDIR, open_errno("errdir", "r"));
    ASSERT(EISDIR, open_errno("errdir", "w"));
    ASSERT(EISDIR, open_errno("errdir", "a"));
    ASSERT(EISDIR, open_errno("errdir/", "r"));
    ASSERT(EISDIR, open_errno("anything/", "w"));     // a name ending in '/' is a folder's
    // a path through a file
    f = fopen("errfile.txt", "w");
    fclose(f);
    ASSERT(ENOTDIR, open_errno("errfile.txt/inner.txt", "r"));
    ASSERT(ENOTDIR, open_errno("errfile.txt/inner.txt", "w"));
    ASSERT(ENOTDIR, open_errno("errfile.txt/deeper/inner.txt", "a"));
    ASSERT(ENOENT, open_errno("errdir/no such file.txt", "r"));
    ASSERT(ENOENT, open_errno("no such folder/x.txt", "r"));
    // remove and rename say why too
    errno = 0;
    ASSERT(-1, remove("no such file.txt"));
    ASSERT(ENOENT, errno);
    errno = 0;
    ASSERT(-1, remove("errfile.txt/inner.txt"));
    ASSERT(ENOTDIR, errno);
    errno = 0;
    ASSERT(-1, rename("no such file.txt", "other.txt"));
    ASSERT(ENOENT, errno);
    ASSERT(0, remove("errfile.txt"));
    ASSERT(0, remove("errdir/a.txt"));
#ifdef _WIN32
    system("rmdir errdir");
#endif
    // the folder went with its last file
    ASSERT(ENOENT, open_errno("errdir", "r"));

    // too many open files
    FILE *open[600];
    int n = 0;
    errno = 0;
    while (n < 600 && (open[n] = tmpfile()) != 0)
        n++;
    ASSERT(EMFILE, errno);
    ASSERT(1, n >= 64 && n < 600);              // FOPEN_MAX at least
    errno = 0;
    ASSERT(1, fopen("errmore.txt", "w") == 0);
    ASSERT(EMFILE, errno);
    fclose(open[--n]);
    f = fopen("errmore.txt", "w");                  // one closed: room for one
    ASSERT(1, f != 0);
    fclose(f);
    while (n > 0)
        fclose(open[--n]);
    remove("errmore.txt");

#ifdef _WIN32                           // Windows: cmd.exe runs the command, its code comes back
    ASSERT(1, system(NULL) != 0);
    ASSERT(3, system("exit 3"));
#else
    // no command processor
    ASSERT(0, system(NULL));
    errno = 0;
    ASSERT(-1, system("dir"));
    ASSERT(ENOSYS, errno);
#endif
    // raise with no such signal
    errno = 0;
    ASSERT(-1, raise(1000));
    ASSERT(EINVAL, errno);
    return g_fail;
}
