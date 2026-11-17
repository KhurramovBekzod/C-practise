/* uniq [-c | -d | -u] [INPUT [OUTPUT]]   removes ADJACENT duplicate lines
   -c prefix count   -d only repeated lines   -u only unique lines
   no INPUT (or "-") -> stdin; OUTPUT file optional.
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

static int opt_c, opt_d, opt_u, out_fd = 1;
static char prev[MAX_LINE + 1];
static size_t prev_len;
static long prev_count;                      /* 0 = no previous line yet */

static void emit(void)
{
    if (prev_count == 0) return;
    if (opt_d && prev_count < 2) return;
    if (opt_u && prev_count > 1) return;
    if (opt_c) { write_num(out_fd, prev_count, 7); write_all(out_fd, " ", 1); }
    write_all(out_fd, prev, prev_len);
    write_all(out_fd, "\n", 1);
}

static int on_line(char *line, size_t len, int has_nl)
{
    (void)has_nl;
    if (prev_count > 0 && len == prev_len && memcmp(line, prev, len) == 0) {
        ++prev_count;                        /* same as previous: just count */
        return 0;
    }
    emit();                                  /* previous group finished */
    memcpy(prev, line, len);
    prev_len = len;
    prev_count = 1;
    return 0;
}

int main(int argc, char **argv)
{
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; ++i)
        for (const char *p = argv[i] + 1; *p; ++p) {
            if      (*p == 'c') opt_c = 1;
            else if (*p == 'd') opt_d = 1;
            else if (*p == 'u') opt_u = 1;
            else return 1;
        }

    int in_fd = 0;
    if (i < argc && strcmp(argv[i], "-") != 0) {
        in_fd = open(argv[i], O_RDONLY);
        if (in_fd < 0) return 1;
    }
    if (i < argc) ++i;
    if (i < argc) {
        out_fd = open(argv[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) return 1;
    }

    for_each_line(in_fd, on_line);
    emit();                                  /* last group */
    if (in_fd != 0) close(in_fd);
    if (out_fd != 1) close(out_fd);
    return 0;
}
