#include "tui.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
// @linux
#include <unistd.h>
#include <sys/ioctl.h>

// for reference, see https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797

#define A_LGREEN "\e[92m"
#define A_GREEN "\e[32m"
#define A_CYAN "\e[36m"
#define A_YELL "\e[33m"
#define A_BOLD "\e[1m"
#define A_RESET "\e[0m"

#define iHEIGHT 4
#define sHEIGHT "4"

bool tty = false;

void tui_backprompt() {
	if (!tty) return;
	printf("\e[1A\e[2K\e[4G");
}

void tui_prompt() {
	if (!tty) return;
	printf("\e[2K\e[4G");
}

void tui_init() {
	tty = isatty(fileno(stdout));
	if (!tty) return;

	for (int i = 0; i < iHEIGHT + 1; i++) {
		putc('\n', stdout);
	}
	printf("\e[1A");
	tui_prompt();
	fflush(stdout);
}

void tui_stop() {
	if (!tty) return;
	printf("\e[0G\e[=7h");
}


typedef struct {
	char* buffer;
	int buff_size;
	int cursor;

	int max_width;
	int width;
} StringBuilder;

void push_string(StringBuilder* b, const char* str) {
	if (str == NULL) str = "(unkown)";

	int len = strlen(str);
	if (b->cursor + len >= b->buff_size) len = b->buff_size - b->cursor;
	if (b->width + len >= b->max_width) len = b->max_width - b->width;
	if (len <= 0) return;

	memcpy(b->buffer + b->cursor, str, len);
	b->cursor += len;
	b->width += len;
}

void push_control(StringBuilder* b, const char* ctrl) {
	int len = strlen(ctrl);
	if (b->cursor + len >= b->buff_size) len = b->buff_size - b->cursor;

	if (len <= 0) return;

	memcpy(b->buffer + b->cursor, ctrl, len);
	b->cursor += len;
}


void tui_draw(TuiData* data) {
	if (!tty) return;
	printf("\e7"); // save cursor

	printf("\e[2G");
	printf(*data->pasused ? A_YELL"⏸"A_RESET : A_GREEN"⏵"A_RESET);

	printf("\e["sHEIGHT"A"); // up n lines
	printf("\n");

	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	char line_buffer[500];
	StringBuilder b = {
		.buffer = line_buffer,
		.buff_size = sizeof(line_buffer),
		.max_width = w.ws_col - 4,
		.cursor = 0,
		.width = 0,
	};

	printf("\e[2G\e[2K");
	push_string(&b, "Previously    ");
	push_control(&b, A_LGREEN);
	push_string(&b, data->prev_meta.title);
	push_control(&b, A_RESET);
	push_string(&b, " by ");
	push_control(&b, A_LGREEN);
	push_string(&b, data->prev_meta.author);
	push_control(&b, A_RESET);
	if (b.width == b.max_width) push_control(&b, A_CYAN"..."A_RESET);
	b.buffer[b.cursor] = 0;
	printf("%s\n", b.buffer);
	b.cursor = b.width = 0;

	printf("\e[2G\e[2K");
	push_string(&b, "Now playing   ");
	push_control(&b, A_GREEN);
	push_string(&b, data->cur_meta.title);
	push_control(&b, A_RESET);
	push_string(&b, " by ");
	push_control(&b, A_GREEN);
	push_string(&b, data->cur_meta.author);
	push_control(&b, A_RESET);
	if (b.width == b.max_width) push_control(&b, A_CYAN"..."A_RESET);
	b.buffer[b.cursor] = 0;
	printf("%s\n", b.buffer);
	b.cursor = b.width = 0;

	printf("\e[2G"); // start of line
	printf("\e[2K"); // clear line
	printf("Volume        "A_BOLD""A_GREEN"% 4d"A_RESET, (int)(*data->volume * 100));

	printf("\e8"); // restore cursor

	fflush(stdout);
}


