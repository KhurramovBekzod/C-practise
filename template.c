/* TEMPLATE: start any new "read input line by line" utility from here.
   Usage as written: ./template [FILE...]  -> prints every line (like cat).
   Change on_line() to do the real work; keep the rest.

   Recipe:
     1. parse flags / arguments in main()
     2. for each input: open(O_RDONLY) -> for_each_line(fd, on_line) -> close
        (no files -> fd 0 = stdin)
     3. in on_line(): decide, then write_all(1, ...) to print
     4. print totals after the loop with write_num(1, value, width)        */

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

/* Call on_line() for every line read from fd.
   line   : the line WITHOUT '\n', null-terminated (safe for strstr/strcmp)
   len    : its length
   has_nl : 1 if the line ended with '\n' (0 only for a last line without it)
   If on_line returns non-zero, reading stops early (e.g. head).          */
typedef int (*line_fn)(char *line, size_t len, int has_nl);

static inline int for_each_line(int fd, line_fn on_line)
{
    static char line[MAX_LINE + 2];   /* +2: room for '\n' and '\0' */
    char buf[BUF_SIZE];
    size_t len = 0;
    ssize_t n;

    while ((n = read(fd, buf, BUF_SIZE)) > 0) {      /* 0 = end of file */
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                line[len] = '\0';
                if (on_line(line, len, 1)) return 1;
                len = 0;
            } else if (len < MAX_LINE) {
                line[len++] = buf[i];
            }
        }
    }
    if (len > 0) {                                    /* last line, no '\n' */
        line[len] = '\0';
        if (on_line(line, len, 0)) return 1;
    }
    return 0;
}

/* ============================== program ============================== */

static long lineno = 0;

static int on_line(char *line, size_t len, int has_nl)
{
    ++lineno;
    /* ---- YOUR LOGIC HERE ----
       line   : text without '\n', already '\0'-terminated
       len    : its length
       has_nl : 1 if it ended with '\n'
       return 1 to stop reading early, 0 to continue                     */
    if (has_nl) line[len++] = '\n';
    write_all(1, line, len);
    return 0;
}

int main(int argc, char **argv)
{
    int rc = 0;
    if (argc == 1) {
        for_each_line(0, on_line);                    /* stdin */
    } else {
        for (int i = 1; i < argc; ++i) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) { rc = 1; continue; }
            for_each_line(fd, on_line);
            close(fd);
        }
    }
    /* print totals here, e.g.: write_num(1, lineno, 0); write_str(1, "\n"); */
    return rc;
}
