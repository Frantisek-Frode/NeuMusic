#pragma once
#include <stdlib.h>

#define failif(cond, ...) if (cond) { fprintf(stderr, __VA_ARGS__); goto FAIL; }

typedef enum {
	ACTION_NEXT,
	ACTION_PREV,
	ACTION_PAUSE,
	ACTION_PLAYPAUSE,
	ACTION_STOP,
	ACTION_PLAY,

	ACTION_COUNT,
} PlayerAction;

typedef struct {
	const char* author;
	const char* title;
} Metadata;

void free_meta(Metadata* meta);

