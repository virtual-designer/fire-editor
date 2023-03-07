#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <curses.h>
#include <sys/ttydefaults.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char *s;
    ssize_t len;
} fire_editor_row_t;

typedef struct {
    ssize_t rows, cols;
    ssize_t offset;
    ssize_t cursor_pos;
    ssize_t _lines;
    ssize_t line_count;
    ssize_t curx, cury;
    ssize_t current_linelen;
    char *program_name;
    fire_editor_row_t *lines;
} fire_config_t;

typedef struct {
    char *content;
    ssize_t len;
} fire_buffer_t;

fire_config_t config;
fire_buffer_t buffer = { NULL, 0 };

FILE *logbuffer;

void 
fire_error(char *err, bool fetch_msg) 
{
    endwin();
    printf("%s: %s%s%s\n", config.program_name, err, fetch_msg ? ": " : "", fetch_msg ? strerror(errno) : "");
    exit(-1);
}

bool 
fire_buffer_append(fire_buffer_t *buffer, char *s) 
{
    ssize_t len = strlen(s);
    char *new_pointer = realloc(buffer->content, buffer->len + (len * sizeof (char)));

    if (new_pointer == NULL) {
        fire_error("failed to reallocate memory", true);
        return false;
    }

    memcpy(new_pointer + buffer->len, s, len);

    buffer->content = new_pointer;
    buffer->len += len;
    
    return true;
}

void
fire_buffer_free(fire_buffer_t *buffer) {
    if (buffer->content != NULL)
        free(buffer->content);
}

void
printw_spaces(ssize_t count) 
{
    for (ssize_t i = 0; i < (count < 0 ? 0 : count); i++) {
        printw(" ");
    }
}

void
printw_padding(ssize_t left, ssize_t right, char *s) 
{
    printw_spaces(left);
    printw("%s", s);
    printw_spaces(right == -1 ? (config.cols - strlen(s) - left) : right);
}

void
fire_draw_statusbar() 
{
    attron(COLOR_PAIR(1));
    printw_padding(3, -1, "main.c -- [modified]");
    attroff(COLOR_PAIR(1));
}

void fire_setup_color_pairs() 
{
    init_pair(1, -1, COLOR_WHITE);
}

void
fire_rows_init(FILE *file) 
{
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    ssize_t line_count = 1, i = 0;

    fire_buffer_append(&buffer, "");
    
    while ((linelen = getline(&line, &linecap, file)) != -1) {
        fire_buffer_append(&buffer, line);
        fire_buffer_append(&buffer, "\n");

        config.lines = realloc(config.lines, sizeof (fire_editor_row_t) * (i + 1));

        if (config.lines == NULL) {
            fire_error("realloc failed", true);
        }

        config.lines[i].len = linelen;
        config.lines[i].s = strdup(line);

        line_count++;
        i++;
    }
        
    free(line);
    line = NULL;
    config.line_count = line_count;
}

void
fire_render_rows() 
{
    clear();
    fire_draw_statusbar();

    fire_buffer_t buffer = { NULL, 0 };

    for (ssize_t i = config.offset; i < (config.line_count - 1) && i < (config.rows + config.offset - 1); i++) {    
        fire_buffer_append(&buffer, config.lines[i].s);
    }

    char c = buffer.content[buffer.len];
    buffer.content[buffer.len] = '\0';
    
    printw("%s\n", buffer.content == NULL ? "" : buffer.content);

    buffer.content[buffer.len] = c;
    fire_buffer_free(&buffer);
}

void 
fire_get_winsize(ssize_t *rows, ssize_t *cols)
{
    struct winsize ws;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        fire_error("ioctl failed", true);
    }

    *rows = ws.ws_row;
    *cols = ws.ws_col;

    printf("Rows: %d, Cols: %d\n", ws.ws_row, ws.ws_col);
}

void 
fire_init(char *program_name) 
{
    config.rows = 0;
    config.cols = 0;
    config.offset = 0;
    config.cursor_pos = 0;
    config._lines = 0;
    config.current_linelen = 0;
    config.line_count = 0;
    config.lines = NULL;

    config.program_name = strdup(program_name);

    fire_get_winsize(&config.rows, &config.cols);

    initscr();

    use_default_colors();
    start_color();

    raw();
    noecho();

    keypad(stdscr, true);

    logbuffer = fopen("fire.log", "a+");
    ftruncate(logbuffer->_fileno, 1024);
}

void 
fire_end() 
{
    endwin();
}

void
fire_read_keys()
{
    int c;

    config.curx = getcurx(stdscr);
    config.cury = getcury(stdscr);

    while ((c = getch()) != CTRL('x')) {
        switch (c) {
            case KEY_UP:
                if (config.offset > 0 && config.cury < 2) {
                    config.offset--;
                    fire_render_rows();
                    move(config.cury, config.curx);
                }
                else if (config.cury > 1) {
                    move(--config.cury, config.curx);
                }
            break;

            case KEY_DOWN:
                if (config.cury >= (config.rows - 1) && (config.cury + config.offset) < (config.line_count)) {
                    config.offset++;
                    fire_render_rows();
                    move(config.cury, config.curx);
                }
                else if ((config.cury + config.offset) <= (config.line_count - 1)) {
                    move(++config.cury, config.curx);
                }
            break;

            case KEY_LEFT:

            break;
            case KEY_RIGHT:
                
            break;

            default:
                printw("%c", c);
            break;
        }
    }
}

void
fire_atexit() {
    fire_buffer_free(&buffer);
}

int 
main(int argc, char **argv) 
{
    if (argc <= 1) {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    FILE *file = fopen(argv[1], "r");

    if (file == NULL) {
        fire_error("could not open input file", true);
    }

    atexit(&fire_atexit);
    fire_init(argv[0]);
    fire_setup_color_pairs();
    fire_rows_init(file);
    fire_render_rows();
    move(1, 0);
    
    config.curx = getcurx(stdscr);
    config.cury = getcury(stdscr);
    
    fire_read_keys();
    fire_end();

    return 0;
}