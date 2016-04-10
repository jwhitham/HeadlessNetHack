#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <dlfcn.h>

#include <termios.h>
#include <unistd.h>

#include "hook.h"

static const char * h_header = "recordversion";
static int h_version = 1003;
static unsigned h_count = 0;
static int h_enable_putchar = 1;

/* Curses thing: */
short int ospeed = 0;

/* Forward decls */
static void h_insist (const char * tag, int value);
static int h_playback (void);


/*********** Part 1: functions that are easily made deterministic *********/
time_t time (time_t *__timer)
{
    if (__timer != NULL) {
        (* __timer) = 1234;
    }
    return 1234;
}

struct tm *h_localtime(const time_t *timep)
{
    static struct tm current_time;

    current_time.tm_sec = 1234 % 60;
    current_time.tm_min = 1234 / 60;
    current_time.tm_hour = 23;
    current_time.tm_year = 1969;
    current_time.tm_wday = 3; /* 31st Dec 1969 was a Wednesday */
    current_time.tm_mon = 12;
    current_time.tm_mday = 31;
    current_time.tm_yday = 365;
    current_time.tm_isdst = 0;
    return &current_time;
}

unsigned int sleep (unsigned int __seconds)
{
    return 0;
}

__pid_t getpid (void)
{
    return 1234;
}

__uid_t getuid (void)
{
    return 1234;
}

__gid_t getgid (void)
{
    return 1234;
}


int setuid (__uid_t __uid)
{
    return 0;
}

int setgid (__gid_t __gid)
{
    return 0;
}

__pid_t fork (void)
{
    return -1;
}

__sighandler_t signal (int __sig, __sighandler_t __handler)
{
    __sighandler_t x;
    memset (&x, 0, sizeof (__sighandler_t));
    return x;
}


int kill (__pid_t __pid, int __sig)
{
    exit (1);
}


__pid_t wait (int * __stat_loc)
{
    return -1;
}

char *getlogin(void)
{
    static char * gl = "magic";
    return gl;
}

char *getenv(const char *name)
{
    static char * term = "linux";
    static char * home = "/";
    if (strcmp (name, "TERM") == 0) {
        return term;
    }
    if (strcmp (name, "HOME") == 0) {
        return home;
    }
    return NULL;
}

int execl (__const char *__path, __const char *__arg, ...) 
{
    return -1;
}

speed_t cfgetospeed (__const struct termios *__termios_p)
{
    return B38400;
}

_Bool has_colors (void)
{
    return 0;
}

int isatty (int __fd)
{
    return (__fd <= 2);
}

__mode_t umask (__mode_t __mask)
{
    return __mask;
}


/*********** Part 2: hook functions *********/

typedef enum {unknown, playback, recording} t_h_state;
static t_h_state h_state = unknown;
static FILE * h_log = NULL;
static void * h_libc6 = NULL;
static void * h_libncurses = NULL;

static void * h_redirector (const char * name)
{
    void * r;

    if ((h_libncurses == NULL) || (h_libc6 == NULL)) {
        fprintf (stderr, "lib handles not dlopen'ed");
        exit (1);
    }
    r = dlsym (h_libncurses, name);
    if (r == NULL) {
        r = dlsym (h_libc6, name);
    }
    if (r == NULL) {
        fprintf (stderr, "symbol '%s' not found in any lib\n", name);
        exit (1);
    }
    return r;
}

static void h_put_int (const char * tag, int x)
{
    if (h_state != recording) {
        fprintf (stderr, "h_put_int outside of recording state (%s)\n", tag);
        exit (1);
    }
    h_count ++;
    fprintf (h_log, "%s %d\n", tag, x);
    fflush (h_log);
}

static void h_put_lump (const char * tag, const void * lump, size_t size)
{
    const char * lump2 = (const char *) lump;
    size_t i;

    h_put_int (tag, lump2[0]);

    for (i = 1; i < size; i++) {
        h_put_int ("x", lump2[i]);
    }
}

static void h_put_str (const char * tag, const char * buf)
{
    size_t size;
   
    if (buf == NULL) {
        h_put_int (tag, -1);
        return;
    }
    size = strlen (buf);

    if (size >= BUFSIZ) {
        fprintf (stderr, "h_put_str: string too long\n");
        exit (1);
    }
    h_put_int (tag, (int) size);
    h_put_lump ("str", buf, size);
}

static int h_get_int (const char * tag)
{
    int x;
    char tag2[32];

    if (h_state != playback) {
        fprintf (stderr, "h_get_int outside of playback state (%s)\n", tag);
        exit (1);
    }
    h_count ++;
    if (fscanf (h_log, "%20s %d", &tag2, &x) != 2)  {
        fprintf (stderr, "h_get_int malformed line %u (%s)\n", h_count, tag);
        exit (1);
    }
    if (strcmp (tag2, tag) != 0) {
        fprintf (stderr, "h_get_int bad tag on line %u (expect %s got %s)\n", h_count, tag, tag2);
        exit (1);
    }
    return x;
}

static void h_get_lump (const char * tag, void * lump, size_t size)
{
    char * lump2 = (char *) lump;
    size_t i;

    lump2[0] = h_get_int (tag);

    for (i = 1; i < size; i++) {
        lump2[i] = h_get_int ("x");
    }
}

static char * h_get_str (const char * tag, char * buf)
{
    size_t size;
    int x;

    x = h_get_int (tag);
    if (x == -1) {
        return NULL;
    }
    size = (size_t) x;
    if (size >= BUFSIZ) {
        fprintf (stderr, "h_get_str: string too long\n");
        exit (1);
    }
    h_get_lump ("str", buf, size);
    buf[size] = '\0';
    return buf;
}

void h_init (const char * log_name, int do_recording, int enable_putchar)
{
    FILE * fd;

    if (h_state != unknown) {
        fprintf (stderr, "h_init outside of unknown state (called twice?)\n");
        exit (1);
    }

    if (do_recording) {
        h_state = recording;
        h_log = fopen (log_name, "wt");
        if (h_log == NULL) {
            perror ("unable to create recording file");
            exit (1);
        }

        h_libc6 = dlopen ("libc.so.6", RTLD_LAZY);
        if (h_libc6 == NULL) {
            perror ("unable to dlopen libc6 (needed for recording)");
            exit (1);
        }
        h_libncurses = dlopen ("libncurses.so.5", RTLD_LAZY);
        if (h_libncurses == NULL) {
            perror ("unable to dlopen libncurses (needed for recording)");
            exit (1);
        }
        h_enable_putchar = 1;
    } else {
        h_log = fopen (log_name, "rt");
        if (h_log == NULL) {
            perror ("unable to open playback file");
        }
        h_state = playback;
        h_enable_putchar = enable_putchar;
    }
    h_insist (h_header, h_version);
}

static void h_insist (const char * tag, int value)
{
    int x;

    if (h_playback()) {
        x = h_get_int (tag);
        if (x != value) {
            fprintf (stderr, "h_insist: %s is wrong, "
                "expect %d got %d\n", tag, value, x);
            exit (1);
        }
    } else {
        h_put_int (tag, value);
    }
}

static int h_playback (void)
{
    if (h_state == unknown) {
    }
    return (h_state == playback);
}

#define redirector(r_xxx, s_xxx, args)                                      \
    if (h_playback ()) {                                                    \
        x = h_get_int (s_xxx);                                              \
    } else {                                                                \
        if (r_xxx == NULL) {                                                \
            r_xxx = h_redirector (s_xxx);                                   \
        }                                                                   \
        x = r_xxx args;                                                     \
        h_put_int (s_xxx, x);                                               \
    }                                                                       \
    return x;

#define redirector_lump(r_xxx, s_xxx, val, size, args)                      \
    if (h_playback ()) {                                                    \
        x = h_get_int (s_xxx ".i");                                         \
        h_get_lump (s_xxx ".l", val, size);                                 \
    } else {                                                                \
        if (r_xxx == NULL) {                                                \
            r_xxx = h_redirector (s_xxx);                                   \
        }                                                                   \
        x = r_xxx args;                                                     \
        h_put_int (s_xxx ".i", x);                                          \
        h_put_lump (s_xxx ".l", val, size);                                 \
    }

#define redirector_str(r_xxx, s_xxx, args)                                  \
    if (h_playback ()) {                                                    \
        static char buf[BUFSIZ];                                            \
        return h_get_str (s_xxx, buf);                                      \
    } else {                                                                \
        char *buf;                                                          \
        if (r_xxx == NULL) {                                                \
            r_xxx = h_redirector (s_xxx);                                   \
        }                                                                   \
        buf = r_xxx args;                                                   \
        h_put_str (s_xxx, buf);                                             \
        return buf;                                                         \
    }

/*********** Part 3: functions that are logged and redirected *********/


int getchar (void)
{
    int x;
    static int (* r_getchar) (void) = NULL;

    redirector (r_getchar, "getchar", ());
}

int tcgetattr (int __fd, struct termios *__termios_p)
{
    int x;
    static int (* r_tcgetattr) (int, struct termios *) = NULL;
    
    redirector_lump (r_tcgetattr, "tcgetattr", __termios_p, sizeof (struct termios), (__fd, __termios_p));
    return x;
}


int tcsetattr (int __fd, int __optional_actions, __const struct termios *__termios_p)
{
    int x;
    static int (* r_tcsetattr) (int, int, __const struct termios *) = NULL;

    redirector(r_tcsetattr, "tcsetattr", (__fd, __optional_actions, __termios_p))
}

int tgetent(char *bp, char *name)
{
    int x;
    static int (* r_tgetent) (char *, char *) = NULL;

    h_insist ("tgetent.x", 1);
    redirector(r_tgetent, "tgetent", (bp, name));
}

int tgetflag(char id[2])
{
    int x;
    static int (* r_tgetflag) (char id[2]) = NULL;

    redirector(r_tgetflag, "tgetflag", (id));
}

int tgetnum(char id[2])
{
    int x;
    static int (* r_tgetnum) (char id[2]) = NULL;

    redirector(r_tgetnum, "tgetnum", (id));
}

char *tgetstr(char id[2], char **area)
{
    static char * (* r_tgetstr) (char id[2], char **area) = NULL;

    redirector_str(r_tgetstr, "tgetstr", (id, area));
}

char *tgoto(char *cap, int col, int row)
{
    static char * (* r_tgoto) (char *, int, int) = NULL;

    redirector_str(r_tgoto, "tgoto", (cap, col, row));
}

int tputs(char *str, int affcnt, int (*putc)(void))
{
    int x;
    static int (* r_tputs) (char *, int, int (* putc)(void)) = NULL;

    if (h_playback () && (str != NULL) && h_enable_putchar) {
        int (* putc2) (int) = (void *) putc;
        size_t i;

        for (i = 0; str[i] != '\0'; i++) {
            putc2 (str[i]);
        }
    }
    redirector(r_tputs, "tputs", (str, affcnt, putc));
}

long int fpathconf (int __fd, int __name)
{
    int x;
    static int (* r_fpathconf) (int, int) = NULL;

    h_insist ("fpathconf.name", __name);
    redirector(r_fpathconf, "fpathconf", (__fd, __name));
}

int chmod (__const char *__file, __mode_t __mode)
{
    h_insist ("chmod", __mode);
    return -1;
}

long int glibc_random (void);
void glibc_srandom (unsigned int x);

long int random (void)
{
    long int value = glibc_random ();

    /* h_insist ("random", value); */
    return value;
}

void srandom (unsigned int __seed)
{
    h_insist ("srandom", __seed);
    glibc_srandom (1);
}

/*********** Part 4: output functions which can be disabled *********/
void h_putchar (int x)
{
    if (h_enable_putchar) {
        putchar (x);
    }
}

