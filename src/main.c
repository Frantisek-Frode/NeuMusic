#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "channel.h"
#include "decoder.h"
#include "player.h"

int main(int argc, const char** argv) {
	srand(time(NULL));

	Channel* audio_channel = channel_alloc(44100, 1);

// per file ---->
	// setup decoder
	pthread_t dec_thread;
	DecoderContext dec_args;
	if (0 > decoder_init(argv[1], &dec_args)) {
		goto FAIL;
	}
	dec_args.output = audio_channel,
	pthread_create(&dec_thread, NULL, decode, &dec_args);

	// setup player
	pthread_t play_thread;
	PlayerArgs play_args = {
		.data = &audio_channel->consumers[0],
		.rate = dec_args.codec_params->sample_rate
	};
	pthread_create(&play_thread, NULL, player_play, &play_args);

	// TODO: handle input

	// cleanup
	pthread_join(dec_thread, NULL);
	decoder_free(&dec_args);
	channel_finish_writing(audio_channel);
	pthread_join(play_thread, NULL);
// <---- per file

FAIL:
	channel_free(audio_channel);

	return 0;
}

