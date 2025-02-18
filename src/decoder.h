#pragma once
#include "channel.h"

typedef struct {
	Channel* output;
	double freq;
	int sample_rate;
	double duration;
	const char* file;
} DecoderArgs;

/// Takes `DecoderArgs*` as arguments.
/// Writes samples into `args->output` and closes it when done.
void* decode(void* args);

