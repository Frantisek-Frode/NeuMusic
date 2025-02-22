#include "tui.h"
#include <stdio.h>
#include <stdbool.h>
// @linux
#include <unistd.h>

// for reference, see https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797

#define A_GREEN "\e[32m"
#define A_YELL "\e[33m"
#define A_BOLD "\e[1m"
#define A_RESET "\e[0m"

#define iHEIGHT 3
#define sHEIGHT "3"

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
	printf("\e[0G");
}

void tui_draw(TuiData* data) {
	if (!tty) return;
	printf("\e7"); // save cursor

	printf("\e[2G");
	printf(*data->pasused ? A_YELL"⏸"A_RESET : A_GREEN"⏵"A_RESET);

	printf("\e["sHEIGHT"A"); // up n lines

	printf("\e[2G\e[K");
	// printf("Previously \t%s\n", data->prev_track);
	printf("\n");
	printf("\e[2G\e[K");
	printf("Now playing\t"A_BOLD""A_GREEN"%s\n"A_RESET, data->current_track);

	printf("\e[2G"); // start of line
	printf("\e[2K"); // clear line
	printf("Volume     \t"A_BOLD""A_GREEN"% 4d"A_RESET, (int)(*data->volume * 100));

	printf("\e8"); // restore cursor

	fflush(stdout);
}


