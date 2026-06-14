#ifndef PV_TERMINAL_H
#define PV_TERMINAL_H

/* Logical key codes returned by read_key(). */
enum {
    K_NONE = 0,
    K_UP = 1000, K_DOWN, K_LEFT, K_RIGHT,
    K_HOME, K_END, K_PGUP, K_PGDN,
    K_ENTER, K_ESC, K_BACKSPACE, K_RESIZE,
    K_MOUSE_UP, K_MOUSE_DOWN
};

/* Install SIGWINCH/SIGINT/SIGTERM handlers (no-op on Windows). */
void term_install_signal_handlers(void);

void enter_raw(void);
void leave_raw(void);

/* Switch to the alternate screen (hide cursor, enable wheel reporting). */
void enter_alt_screen(void);
void leave_alt_screen(void);

void term_size(int *rows, int *cols);

/* Read one logical key, blocking with zero idle CPU. */
int read_key(void);

#endif /* PV_TERMINAL_H */
