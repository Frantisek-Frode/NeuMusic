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

void tui_draw(TuiData* data) {
	if (!tty) return;
	printf("\e7"); // save cursor

	printf("\e[2G");
	printf(*data->pasused ? A_YELL"⏸"A_RESET : A_GREEN"⏵"A_RESET);

	printf("\e["sHEIGHT"A"); // up n lines
	printf("\n");

	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	int line_width = w.ws_col;
	char* line_buffer = malloc(line_width);
	if (line_buffer == NULL) {
		fprintf(stderr, "OOM\n");
		exit(-1);
	}

	printf("\e[2G\e[2K");
	snprintf(line_buffer, line_width,
		"Previously \t"A_LGREEN"%s"A_RESET" by "A_LGREEN"%s",
		data->prev_meta.title, data->prev_meta.author);
	memcpy(line_buffer + line_width - 5, "...", 4);
	printf("%s\n"A_RESET, line_buffer);

	printf("\e[2G\e[2K");
	snprintf(line_buffer, line_width,
		"Now playing\t"A_BOLD""A_GREEN"%s"A_RESET" by "A_BOLD""A_GREEN"%s",
		data->cur_meta.title, data->cur_meta.author);
	memcpy(line_buffer + line_width - 5, "...", 4);
	printf("%s\n"A_RESET, line_buffer);

	free(line_buffer);

	printf("\e[2G"); // start of line
	printf("\e[2K"); // clear line
	printf("Volume     \t"A_BOLD""A_GREEN"% 4d"A_RESET, (int)(*data->volume * 100));

	printf("\e8"); // restore cursor

	fflush(stdout);
}


