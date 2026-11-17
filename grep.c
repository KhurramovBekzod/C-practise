/* grep PATTERN [FILE...]   (last year's midterm problem)
   no FILE -> stdin. Prints lines containing PATTERN (plain text, no regex).
   Exit: 0 = something matched, 1 = nothing matched, 2 = bad usage.
   System calls: open, read, write, close                                  */

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

static const char *pattern;
static int found = 0;

static int on_line(char *line, size_t len, int has_nl)
{
    (void)has_nl;                         /* grep always prints a '\n' */
    if (strstr(line, pattern) != NULL) {
        line[len] = '\n';                 /* line + newline in ONE write */
        write_all(1, line, len + 1);
        line[len] = '\0';
        found = 1;
    }
    return 0;                             /* 0 = keep reading */
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;               /* no pattern: non-zero, no message */
    pattern = argv[1];

    if (argc == 2) {
        for_each_line(0, on_line);        /* fd 0 = stdin */
    } else {
        for (int i = 2; i < argc; ++i) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) continue;         /* error handling may be ignored */
            for_each_line(fd, on_line);
            close(fd);
        }
    }
    return found ? 0 : 1;
}
