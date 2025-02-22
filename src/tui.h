#pragma once
#include "comain.h"
#include <stdbool.h>

typedef struct {
	double* volume;
	bool* pasused;
	double* progress;

	Metadata cur_meta;
	Metadata prev_meta;
} TuiData;

void tui_prompt();
void tui_backprompt();
void tui_init();
void tui_draw(TuiData* ctx);
void tui_stop();

