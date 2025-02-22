#pragma once
#include <stdbool.h>

typedef struct {
	double* volume;
	const char* current_track;
	const char* prev_track;
	bool* pasused;
	double* progress;
} TuiData;

void tui_prompt();
void tui_backprompt();
void tui_init();
void tui_draw(TuiData* ctx);
void tui_stop();

