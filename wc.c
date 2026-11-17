/* wc [-l] [-w] [-c] [FILE...]   (flags combinable: -lw)   no FILE -> stdin
   -l lines ('\n' count)  -w words  -c bytes. No flags = all three.
   Output spacing copies GNU wc (numbers right-aligned to a common width).
   System calls: open, read, write, close, fstat (only for the width rule)  */

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
#include <sys/stat.h>   /* fstat: is the input a regular file? */

#define MAX_FILES 256

static int opt_l, opt_w, opt_c;
static long res[MAX_FILES][3];
static const char *names[MAX_FILES];
static int nres, not_regular;

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static void count_fd(int fd, long out[3])
{
    char buf[BUF_SIZE];
    ssize_t n;
    long lines = 0, words = 0, bytes = 0;
    int in_word = 0;
    struct stat st;

    if (fstat(fd, &st) == 0 && !S_ISREG(st.st_mode)) not_regular = 1;
    while ((n = read(fd, buf, BUF_SIZE)) > 0) {
        bytes += n;
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') ++lines;
            if (is_space(buf[i])) in_word = 0;
            else if (!in_word) { in_word = 1; ++words; }   /* a word starts */
        }
    }
    out[0] = lines; out[1] = words; out[2] = bytes;
}

static void print_row(const long v[3], const char *name, int width)
{
    int first = 1;
    int show[3] = { opt_l, opt_w, opt_c };
    for (int k = 0; k < 3; ++k) {
        if (!show[k]) continue;
        if (!first) write_str(1, " ");
        write_num(1, v[k], width);
        first = 0;
    }
    if (name) { write_str(1, " "); write_str(1, name); }
    write_str(1, "\n");
}

int main(int argc, char **argv)
{
    int i = 1, rc = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; ++i)
        for (const char *p = argv[i] + 1; *p; ++p) {
            if      (*p == 'l') opt_l = 1;
            else if (*p == 'w') opt_w = 1;
            else if (*p == 'c') opt_c = 1;
            else return 1;
        }
    if (!opt_l && !opt_w && !opt_c) opt_l = opt_w = opt_c = 1;

    if (i == argc) {                                  /* stdin */
        count_fd(0, res[0]); names[0] = NULL; nres = 1;
    } else {
        for (; i < argc && nres < MAX_FILES; ++i) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) { rc = 1; continue; }
            count_fd(fd, res[nres]);
            names[nres++] = argv[i];
            close(fd);
        }
    }

    long total[3] = { 0, 0, 0 };
    for (int k = 0; k < nres; ++k)
        for (int j = 0; j < 3; ++j) total[j] += res[k][j];

    /* GNU width rule: 1 count + 1 input -> no padding; pipe -> 7;
       otherwise -> number of digits of the total byte count            */
    int width = 1;
    for (long b = total[2]; b >= 10; b /= 10) ++width;
    if (not_regular && width < 7) width = 7;
    if (opt_l + opt_w + opt_c == 1 && nres == 1) width = 0;

    for (int k = 0; k < nres; ++k) print_row(res[k], names[k], width);
    if (nres > 1) print_row(total, "total", width);
    return rc;
}
