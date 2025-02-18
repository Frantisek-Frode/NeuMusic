#include "player.h"
#include "channel.h"
#include <pthread.h>
#include <ao/ao.h>
#include <string.h>
#include <stdint.h>
#include "mpris.h"
#include <stdbool.h>


typedef struct {
	uint16_t left;
	uint16_t right;
} Sample;
#define BUFF_SIZE 500

void* player_play(void* _args) {
	PlayerArgs* args = _args;

	ao_initialize();
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

		ao_play(device, (char*)buffer, buff_size);

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
				goto STOP;
			default: break;
			}
		}
	}

STOP:
	ao_close(device);
	ao_shutdown();

	free(buffer);

	pthread_exit(NULL);
}

