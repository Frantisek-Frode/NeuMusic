#pragma once
#include "channel.h"

typedef struct {
	ChannelConsumer* data;
} PlayerArgs;

void* player_play(void* args);

