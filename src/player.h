#pragma once
#include "channel.h"

typedef struct {
	ChannelConsumer* data;
	int rate;
	ChannelConsumer* events;
	double* volume;
	bool* paused;
} PlayerArgs;

void* player_play(void* args);

