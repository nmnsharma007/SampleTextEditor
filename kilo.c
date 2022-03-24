/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "utils.h"

/*** defines ***/
#define CTRL_KEY(k) ((k)&0x1f)
#define KILO_VERSION "1.0.0"

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};

/*** data ***/
char faculty[6][3] = { "  ", "F1", "F2", "F3", "F4", "  " };
char student[5][3] = { "  ", "S1", "S2", "S3", "S4" };
char data[4][4] = { { '1', '2', '3', '4' }, { '2', '3', '4', '5' }, { '3', '4', '5', '6' }, { '4', '5', '6', '7' } };
char sum[4][10] = { "10", "14", "18", "22" };
int num_rows;
int num_cols;

typedef struct erow {
    int size;
    char *chars;
} erow;

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

int editorReadKey() {
    // int nread;
    // char c;
    // while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    // {
    //         if (nread == -1 && errno != EAGAIN)
    //         {
    //                 die("read");
    //         }
    // }
    // return c;

    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** row operations ***/

void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    ++E.numrows;
}

// void editorRowInsertChar(erow *row, int at, int c) {
//     if (at < 0 || at > row->size) {
//         at = row->size;
//     }
//     row->chars = realloc(row->chars, row->size + 2);
//     memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
//     row->size++;
//     row->chars[at] = c;
//     //        editorUpdateRow(row);
// }

void editorInsertChar(int c) {
    data[E.cy / 4 - 1][E.cx / 6 - 1] = c;
}

/*** file i/o ***/
void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        die("fopen");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            --linelen;
        }
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT \
    {             \
        NULL, 0   \
    }

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) {
        return;
    }
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/
void editorDrawRows(struct abuf *ab) {
    find_sum(sum, data);
    num_rows = 5, num_cols = 6;
    for (int row = 0; row <= num_rows * 4; ++row) {
        if (row % 4 == 0) {
            int len = (num_cols - 1) * 6 + 7;
            for (int col = 0; col <= len; ++col) {
                if (col % 6 == 0) {
                    if (col != len - 1)
                        abAppend(ab, "+", 1);
                    else
                        abAppend(ab, "-", 1);
                } else {
                    if (col == len)
                        abAppend(ab, "+", 1);
                    else
                        abAppend(ab, "-", 1);
                }
            }
        } else {
            int len = (num_cols - 1) * 6 + 7;
            for (int col = 0; col <= len; ++col) {
                if (col % 6 == 0) {
                    if (col == len - 1)
                        abAppend(ab, " ", 1);
                    else
                        abAppend(ab, "|", 1);
                } else if (row > 2 && col == len - 4 && (row + 2) % 4 == 0) {
                    abAppend(ab, sum[row / 4 - 1], 2);
                    col++;
                } else if (col == 3 && (row + 2) % 4 == 0) {
                    abAppend(ab, student[row / 4], 2);
                    col++;
                } else if (row == 2 && (col + 3) % 6 == 0) {
                    abAppend(ab, faculty[col / 6], 2);
                    col++;
                } else if (row > 2 && col > 3 && (col + 3) % 6 == 0 && (row + 2) % 4 == 0) {
                    abAppend(ab, &data[row / 4 - 1][col / 6 - 1], 1);
                } else {
                    if (col == len)
                        abAppend(ab, "|", 1);
                    else
                        abAppend(ab, " ", 1);
                }
            }
        }
        abAppend(ab, "\r\n", 2);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "\nS%d F%d\n", E.cy / 4, E.cx / 6);
    abAppend(ab, buf, strlen(buf));

    /*int y;
    for (y = 0; y < E.screenrows; y++) {
            if (y >= E.numrows) {
                    if (E.numrows == 0 && y == E.screenrows / 3) {
                            char welcome[80];
                            int welcomelen = snprintf(welcome, sizeof(welcome),
                            "Kilo editor -- version %s", KILO_VERSION);
                            if (welcomelen > E.screencols) welcomelen = E.screencols;
                            int padding = (E.screencols - welcomelen) / 2;
                            if (padding) {
                                    abAppend(ab, "~", 1);
                                    padding--;
                            }
                            while (padding--) abAppend(ab, " ", 1);
                            abAppend(ab, welcome, welcomelen);
                    } else {
                            abAppend(ab, "~", 1);
                    }
            } else {
                    int len = E.row[y].size;
                    if (len > E.screencols) len = E.screencols;
                    abAppend(ab, E.row[y].chars, len);
            }
            abAppend(ab, "\x1b[K", 3);
            if (y < E.screenrows - 1) {
                    abAppend(ab, "\r\n", 2);
            }
    }*/
}

void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4);
    int len = 0;
    while (len < E.screencols) {
        abAppend(ab, " ", 1);
        ++len;
    }
    abAppend(ab, "\x1b[m", 3);
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    // editorDrawStatusBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy, E.cx);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/
void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if ((E.cx - 12) > 0) {
                E.cx -= 6;
            }
            break;
        case ARROW_RIGHT:
            if ((E.cx + 6) < ((num_cols - 1) * 6)) {
                E.cx += 6;
            }
            break;
        case ARROW_UP:
            if ((E.cy - 8) > 0) {
                E.cy -= 4;
            }
            break;
        case ARROW_DOWN:
            if ((E.cy + 4) < (num_rows * 4)) {
                E.cy += 4;
            }
            break;
    }
}

void editMode() {
    char prevNum = data[E.cy / 4 - 1][E.cx / 6 - 1];
    editorInsertChar(' ');
    while (1) {
        editorRefreshScreen();
        char c = editorReadKey();
        if (c == 27) {
            editorInsertChar(prevNum);
            break;
        }
        if (c == '\r' && data[E.cy / 4 - 1][E.cx / 6 - 1] != ' ') {
            break;
        }
        switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                editorInsertChar(c);
                break;
        }
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        case 27:
            write(STDOUT_FILENO, "\x1b[24;1H", 7);
            exit(0);
            break;
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
            editorMoveCursor(c);
            break;
        case '\r':
            editMode();
            break;
    }
}

/*** init ***/
void initEditor() {
    E.cx = 10;
    E.cy = 7;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
        die("getWindowSize");
    }
    E.numrows = 0;
    E.row = NULL;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}