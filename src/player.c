#include "player.h"
#include "channel.h"
#include <pthread.h>
#include <ao/ao.h>
#include <string.h>
#include <stdint.h>
#include "mpris.h"
#include <stdbool.h>
#include <math.h>


typedef struct {
	int16_t left;
	int16_t right;
} Sample;
#define BUFF_SIZE 500

void* player_play(void* _args) {
	PlayerArgs* args = _args;

	int default_driver = ao_default_driver_id();

	ao_sample_format format;
	memset(&format, 0, sizeof(format));
	format.bits = 16;
	format.channels = 2;
	format.rate = args->rate;
	format.byte_format = AO_FMT_NATIVE;

	ao_device* device = ao_open_live(default_driver, &format, NULL);
	if (device == NULL) {
		fprintf(stderr, "PLAYER: Error opening device.\n");
		pthread_exit(NULL);
	}

	int buff_size = BUFF_SIZE * sizeof(Sample);
	Sample* buffer = calloc(BUFF_SIZE, sizeof(Sample));

	bool paused = false;
	for (;;) {
		int read = channel_read(args->data, buff_size, buffer);
		if (!read) break;
		for (int i = 0; i < read / sizeof(Sample); i++) {
			buffer[i].left *= exp(args->volume - 1);
			buffer[i].right *= exp(args->volume - 1);
		}

		ao_play(device, (char*)buffer, read);

		// check events
		while (paused || (sizeof(PlayerAction) <= channel_available(args->events))) {
			PlayerAction action;
			channel_read(args->events, sizeof(PlayerAction), &action);

			switch (action) {
			case ACTION_PLAY:
				paused = false;
				break;
			case ACTION_PAUSE:
				paused = true;
				break;
			case ACTION_PLAYPAUSE:
				paused = !paused;
				break;
			case ACTION_STOP:
			case ACTION_NEXT:
			case ACTION_PREV:
				// drain the data channel in order to make the decoder read events
				while (buff_size <= channel_available(args->data)) {
					channel_read(args->data, buff_size, buffer);
				}
				goto STOP;
			default: break;
			}
		}
	}

STOP:
	ao_close(device);

	free(buffer);

	pthread_exit(NULL);
}

