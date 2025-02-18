#pragma once
#include "channel.h"

typedef struct {
	ChannelConsumer* data;
	int rate;
} PlayerArgs;

void* player_play(void* args);

