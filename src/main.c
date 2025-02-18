#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "channel.h"
#include "decoder.h"
#include "player.h"

int main(int argc, const char** argv) {
	srand(time(NULL));

	Channel* channel = channel_alloc(44100, 1);
	double freq_number = (rand() & 0xffff) / (double)0xffff;

	pthread_t dec_thread;
	DecoderArgs dec_args = {
		.output = channel,
		.freq = freq_number * 8000.0 + 100.0,
		.sample_rate = 44100,
		.duration = 1,
		.file = argv[1],
	};
	pthread_create(&dec_thread, NULL, decode, &dec_args);

	pthread_t play_thread;
	PlayerArgs play_args = {
		.data = &channel->consumers[0]
	};
	pthread_create(&play_thread, NULL, player_play, &play_args);

	pthread_join(play_thread, NULL);
	pthread_join(dec_thread, NULL);
	channel_free(channel);

	return 0;
}

