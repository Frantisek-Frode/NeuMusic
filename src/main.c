#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "channel.h"
#include "decoder.h"
#include "mpris.h"
#include "player.h"
#include <unistd.h>
#include <stdbool.h>

enum EventConsumers {
	EC_MAIN = 0,
	EC_DEC,
	EC_PLAY,

	EC_COUNT,
};

int main(int argc, const char** argv) {
	srand(time(NULL));

	Channel* event_channel = channel_alloc(5 * sizeof(PlayerAction), EC_COUNT);
	MprisContext mpris_ctx;
	bool mpris = 0 <= mpris_init(&mpris_ctx);
	mpris_ctx.event_channel = event_channel;
	ChannelConsumer* events = &event_channel->consumers[EC_MAIN];

	Channel* audio_channel = channel_alloc(44100, 1);

// per file ---->
	// setup decoder
	pthread_t dec_thread;
	DecoderContext dec_args;
	if (0 > decoder_init(argv[1], &dec_args)) {
		goto FAIL;
	}
	dec_args.output = audio_channel;
	dec_args.events = &event_channel->consumers[EC_DEC];
	pthread_create(&dec_thread, NULL, decode, &dec_args);

	// setup player
	pthread_t play_thread;
	PlayerArgs play_args = {
		.data = &audio_channel->consumers[0],
		.rate = dec_args.codec_params->sample_rate,
		.events = &event_channel->consumers[EC_PLAY],
	};
	pthread_create(&play_thread, NULL, player_play, &play_args);

	// main loop
	for (;;) {
		if (mpris) {
			mpris_check(&mpris_ctx);
		}
		usleep(10000);

		// handle events
		while (sizeof(PlayerAction) <= channel_available(events)) {
			PlayerAction action;
			channel_read(events, sizeof(PlayerAction), &action);

			switch (action) {
			case ACTION_STOP:
			case ACTION_NEXT:
			case ACTION_PREV:
				// TODO: playlists
				goto BREAK_MAIN;
			default: break;
			}
		}
	}
BREAK_MAIN:

	// cleanup
	pthread_join(dec_thread, NULL);
	decoder_free(&dec_args);
	channel_finish_writing(audio_channel);
	pthread_join(play_thread, NULL);
// <---- per file

FAIL:
	channel_free(audio_channel);

	if (mpris) {
		mpris_free(&mpris_ctx);
	}
	channel_free(event_channel);

	return 0;
}

