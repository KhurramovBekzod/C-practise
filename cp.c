/* cp SOURCE DEST      copies one file (DEST is created or overwritten)
   System calls: open (read + write modes), read, write, close             */

/* ===================== helpers (no hidden system calls) ===================== */
#include <fcntl.h>      /* open, O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, O_APPEND */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* strlen, strcmp, strstr, memcpy  (pure user-mode) */

#define BUF_SIZE 4096   /* bytes per read() */
#define MAX_LINE 4096   /* max stored line length; longer lines are cut */

/* write ALL n bytes (write() may write fewer than asked) */
static inline int write_all(int fd, const char *buf, size_t n)
{
    while (n > 0) {
        ssize_t w = write(fd, buf, n);
        if (w < 0) return -1;
        buf += w;
        n -= (size_t)w;
    }
    return 0;
}

static inline void write_str(int fd, const char *s) { write_all(fd, s, strlen(s)); }

/* number -> text, right-aligned to `width` (0 = no padding) */
static inline void write_num(int fd, long x, int width)
{
    char s[32];
    int i = 32, neg = x < 0;
    unsigned long u = neg ? -(unsigned long)x : (unsigned long)x;
    do { s[--i] = (char)('0' + u % 10); u /= 10; } while (u > 0);
    if (neg) s[--i] = '-';
    while (32 - i < width) s[--i] = ' ';
    write_all(fd, s + i, (size_t)(32 - i));
}

/* text -> number; returns 1 on success, 0 if not a valid non-negative integer */
static inline int parse_num(const char *s, long *out)
{
    long v = 0;
    if (*s == '\0') return 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (*s - '0');
    }
    *out = v;
    return 1;
}

/* ============================== program ============================== */

int main(int argc, char **argv)
{
    if (argc != 3) return 1;

    int in = open(argv[1], O_RDONLY);
    if (in < 0) return 1;

    /* O_WRONLY: write only, O_CREAT: create if missing, O_TRUNC: empty it first.
       0644 = permissions rw-r--r-- (needed whenever O_CREAT is used)          */
    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { close(in); return 1; }

    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(in, buf, BUF_SIZE)) > 0)
        if (write_all(out, buf, (size_t)n) < 0) { close(in); close(out); return 1; }

    close(in);
    close(out);
    return n < 0 ? 1 : 0;
}
