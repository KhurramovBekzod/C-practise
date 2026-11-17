/* grep [-v] [-i] [-c] [-n] PATTERN [FILE...]    (flags can be combined: -vn)
   -v invert match   -i ignore case   -c print count only   -n line numbers
   With 2+ files each output line is prefixed "FILE:" (like real grep).
   Exit: 0 = matched, 1 = nothing matched, 2 = bad usage.                   */

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

static int opt_v, opt_i, opt_c, opt_n, multi;
static const char *pattern;
static char low_pat[MAX_LINE + 1];
static char low_line[MAX_LINE + 2];
static const char *cur_name;
static long lineno, count;
static int found;

static void to_lower(char *dst, const char *src)      /* no <ctype.h> needed */
{
    while ((*dst++ = (*src >= 'A' && *src <= 'Z') ? (char)(*src + 32) : *src)) ++src;
}

static int on_line(char *line, size_t len, int has_nl)
{
    (void)has_nl;
    ++lineno;
    int m;
    if (opt_i) { to_lower(low_line, line); m = strstr(low_line, low_pat) != NULL; }
    else       { m = strstr(line, pattern) != NULL; }
    if (opt_v) m = !m;
    if (!m) return 0;

    found = 1;
    ++count;
    if (opt_c) return 0;                   /* -c: only count, print later */
    if (multi) { write_str(1, cur_name); write_str(1, ":"); }
    if (opt_n) { write_num(1, lineno, 0); write_str(1, ":"); }
    line[len] = '\n';
    write_all(1, line, len + 1);
    line[len] = '\0';
    return 0;
}

static void run(int fd, const char *name)
{
    cur_name = name; lineno = 0; count = 0;
    for_each_line(fd, on_line);
    if (opt_c) {
        if (multi) { write_str(1, name); write_str(1, ":"); }
        write_num(1, count, 0);
        write_str(1, "\n");
    }
}

int main(int argc, char **argv)
{
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; ++i) {
        if (strcmp(argv[i], "--") == 0) { ++i; break; }
        for (const char *p = argv[i] + 1; *p; ++p) {
            if      (*p == 'v') opt_v = 1;
            else if (*p == 'i') opt_i = 1;
            else if (*p == 'c') opt_c = 1;
            else if (*p == 'n') opt_n = 1;
            else return 2;                 /* unknown flag */
        }
    }
    if (i >= argc) return 2;               /* no pattern */
    pattern = argv[i++];
    if (opt_i) to_lower(low_pat, pattern);
    multi = (argc - i) > 1;

    if (i == argc) {
        run(0, "(standard input)");
    } else {
        for (; i < argc; ++i) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) continue;
            run(fd, argv[i]);
            close(fd);
        }
    }
    return found ? 0 : 1;
}
