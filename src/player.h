#pragma once
#include "channel.h"

typedef struct {
	ChannelConsumer* data;
	int rate;
	ChannelConsumer* events;
} PlayerArgs;

void* player_play(void* args);

