#pragma once
#include "channel.h"
#include "comain.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct {
	ChannelConsumer* events;
	Channel* output;
	AVFormatContext* format_ctx;
	AVCodecParameters* codec_params;
	AVCodec* codec;
	int audio_stream;

	Metadata metadata;
} DecoderContext;


int decoder_init(const char* path, DecoderContext* result);
void decoder_free(DecoderContext* ctx);
/// Takes `DecoderContext*` as arguments.
/// Writes samples into `args->output` and closes it when done.
void* decode(void* args);

