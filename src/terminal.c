#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "util.h"

#ifndef _WIN32
static struct termios g_orig_termios;
static int g_raw_active = 0;
static volatile sig_atomic_t g_resized = 0;

void leave_raw(void) {
    if (g_raw_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_active = 0;
    }
}

void leave_alt_screen(void) {
    /* disable mouse, leave alt screen, show cursor */
    const char *s = "\x1b[?1006l\x1b[?1000l\x1b[?1049l\x1b[?25h";
    ssize_t r = write(STDOUT_FILENO, s, strlen(s));
    (void)r;
}

void enter_alt_screen(void) {
    /* alt screen, hide cursor, enable SGR mouse (wheel) reporting */
    const char *s = "\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h";
    ssize_t r = write(STDOUT_FILENO, s, strlen(s));
    (void)r;
}

static void on_winch(int sig) {
    (void)sig;
    g_resized = 1;
}

static void on_term(int sig) {
    leave_alt_screen();
    leave_raw();
    signal(sig, SIG_DFL);
    raise(sig);
}

void term_install_signal_handlers(void) {
    signal(SIGWINCH, on_winch);
    signal(SIGINT, on_term);
    signal(SIGTERM, on_term);
}

void enter_raw(void) {
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == -1) {
        die("not a tty");
    }
    struct termios raw = g_orig_termios;
    /* Raw input but keep ISIG so Ctrl-C still signals (and we restore). */
    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("failed to set raw mode");
    }
    g_raw_active = 1;
}

void term_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

int read_key(void) {
    unsigned char c;
    for (;;) {
        struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
        int pr = poll(&pfd, 1, -1);
        if (pr < 0) {
            if (g_resized) {
                g_resized = 0;
                return K_RESIZE;
            }
            continue;
        }
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            if (g_resized) {
                g_resized = 0;
                return K_RESIZE;
            }
            continue;
        }
        break;
    }

    if (c == '\r' || c == '\n') {
        return K_ENTER;
    }
    if (c == 127 || c == 8) {
        return K_BACKSPACE;
    }
    if (c != 0x1b) {
        return c;
    }

    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    if (poll(&pfd, 1, 20) <= 0) {
        return K_ESC;
    }
    unsigned char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) {
        return K_ESC;
    }
    if (seq[0] != '[' && seq[0] != 'O') {
        return K_ESC;
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) {
        return K_ESC;
    }
    if (seq[0] == '[' && seq[1] == '<') {
        /* SGR mouse: ESC [ < btn ; x ; y (M|m) */
        char mbuf[32];
        int mi = 0;
        for (;;) {
            unsigned char mc;
            if (read(STDIN_FILENO, &mc, 1) != 1) {
                return K_NONE;
            }
            if (mc == 'M' || mc == 'm') {
                break;
            }
            if (mi < (int)sizeof(mbuf) - 1) {
                mbuf[mi++] = (char)mc;
            }
        }
        mbuf[mi] = '\0';
        int btn = atoi(mbuf);
        if (btn == 64) {
            return K_MOUSE_UP;
        }
        if (btn == 65) {
            return K_MOUSE_DOWN;
        }
        return K_NONE;
    }
    if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1 || seq[2] != '~') {
            return K_ESC;
        }
        switch (seq[1]) {
            case '1': return K_HOME;
            case '4': return K_END;
            case '5': return K_PGUP;
            case '6': return K_PGDN;
            default: return K_ESC;
        }
    }
    switch (seq[1]) {
        case 'A': return K_UP;
        case 'B': return K_DOWN;
        case 'C': return K_RIGHT;
        case 'D': return K_LEFT;
        case 'H': return K_HOME;
        case 'F': return K_END;
        default: return K_ESC;
    }
}
#else /* _WIN32 */
static DWORD g_orig_out_mode, g_orig_in_mode;
static HANDLE g_hout, g_hin;

void term_install_signal_handlers(void) {
    /* No-op on Windows. */
}

void enter_raw(void) {
    g_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    g_hin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(g_hout, &g_orig_out_mode);
    GetConsoleMode(g_hin, &g_orig_in_mode);
    SetConsoleMode(g_hout,
                   g_orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleMode(g_hin,
                   g_orig_in_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
}

void leave_raw(void) {
    SetConsoleMode(g_hout, g_orig_out_mode);
    SetConsoleMode(g_hin, g_orig_in_mode);
}

void enter_alt_screen(void) {
    fputs("\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h", stdout);
    fflush(stdout);
}

void leave_alt_screen(void) {
    fputs("\x1b[?1006l\x1b[?1000l\x1b[?1049l\x1b[?25h", stdout);
    fflush(stdout);
}

void term_size(int *rows, int *cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_hout, &csbi)) {
        *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

int read_key(void) {
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        int c2 = _getch();
        switch (c2) {
            case 72: return K_UP;
            case 80: return K_DOWN;
            case 75: return K_LEFT;
            case 77: return K_RIGHT;
            case 71: return K_HOME;
            case 79: return K_END;
            case 73: return K_PGUP;
            case 81: return K_PGDN;
            default: return K_NONE;
        }
    }
    if (c == '\r' || c == '\n') return K_ENTER;
    if (c == 8) return K_BACKSPACE;
    if (c == 27) return K_ESC;
    return c;
}
#endif /* _WIN32 */
