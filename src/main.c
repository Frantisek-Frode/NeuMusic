#include "channel.h"
#include "comain.h"
#include "decoder.h"
#include "djay.h"
#include "input.h"
#include "mpris.h"
#include "player.h"
#include "tui.h"
#include <pthread.h>
#include <unistd.h>
#include <ao/ao.h>

const char* cpl_flag = "--create-playlist";
#define usage_play "%s PLAYLIST\n", exe_name
#define usage_cpl "%s %s DIR [FILE_NAME]\n", exe_name, cpl_flag

typedef struct {
	enum {
		MODE_CPL,
		MODE_PLAY,
	} mode;
	union {
		struct {
			const char* dir;
			const char* filename;
		} cpl;
		const char* playlist;
	} data;
} Args;

Args parse_args(int argc, const char** argv) {
	int last_slash = -1;
	for (int i = 0; argv[0][i]; i++) {
		if (argv[0][i] == '/') last_slash = i;
	}
	const char* exe_name = argv[0] + last_slash + 1;

	if (argc < 2) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, usage_play);
		fprintf(stderr, usage_cpl);
		exit(-1);
	}

	if (0==strcmp(argv[1], cpl_flag)) {
		if (argc != 3 && argc != 4) {
			fprintf(stderr, "Usage: ");
			fprintf(stderr, usage_cpl);
			exit(-1);
		}

		Args ret = {
			.mode = MODE_CPL,
			.data.cpl.dir = argv[2],
			.data.cpl.filename = NULL,
		};
		if (argc == 4) ret.data.cpl.filename = argv[3];

		return ret;
	} else {
		if (argc != 2) {
			fprintf(stderr, "Usage: ");
			fprintf(stderr, usage_play);
			exit(-1);
		}

		Args ret = {
			.mode = MODE_PLAY,
			.data.playlist = argv[1],
		};

		return ret;
	}
}

enum EventConsumers {
	EC_MAIN = 0,
	EC_DEC,
	EC_PLAY,

	EC_COUNT,
};

int main(int argc, const char** argv) {
	Args args = parse_args(argc, argv);
	if (args.mode == MODE_CPL) {
		const char* playlist = djay_create_playlist(args.data.cpl.dir, args.data.cpl.filename);
		if (playlist == NULL) return -1;

		printf("Created playlist '%s'\n", playlist);
		free((void*)playlist);
		return 0;
	}

	srand(time(NULL));

	DJayContext djay_ctx;
	if (0 > djay_init(args.data.playlist, &djay_ctx)) return -1;

	Channel* event_channel = channel_alloc(5 * sizeof(PlayerAction), EC_COUNT);
	MprisContext mpris_ctx;
	bool mpris = 0 <= mpris_init(&mpris_ctx);
	mpris_ctx.event_channel = event_channel;
	ChannelConsumer* events = &event_channel->consumers[EC_MAIN];
	Channel* audio_channel = channel_alloc(44100, 1);

	bool paused = false;
	double volume = -1;
	ao_initialize();

	Metadata empty_meta = {
		.author = NULL,
		.title = NULL,
	};

	TuiData tui_data = {
		.volume = &volume,
		.pasused = &paused,
		.cur_meta = empty_meta,
		.prev_meta = empty_meta,
	};

	tui_init();

	bool stop = false;
	while (!stop) {
		// setup decoder
		pthread_t dec_thread;
		DecoderContext dec_args;
		if (0 > decoder_init(djay_ctx.current_path, &dec_args)) {
			goto FAIL;
		}
		dec_args.output = audio_channel;
		dec_args.events = &event_channel->consumers[EC_DEC];
		pthread_create(&dec_thread, NULL, decode, &dec_args);
		tui_data.cur_meta = dec_args.metadata;

		// setup player
		pthread_t play_thread;
		PlayerArgs play_args = {
			.data = &audio_channel->consumers[0],
			.rate = dec_args.codec_params->sample_rate,
			.events = &event_channel->consumers[EC_PLAY],
			.volume = &volume,
			.paused = &paused,
		};
		pthread_create(&play_thread, NULL, player_play, &play_args);

		// ui loop
		while (!audio_channel->finished) {
			input_stdin(event_channel, &volume);
			if (mpris) mpris_check(&mpris_ctx);

			// handle events
			while (sizeof(PlayerAction) <= channel_available(events)) {
				PlayerAction action;
				channel_read(events, sizeof(PlayerAction), &action);

				switch (action) {
				case ACTION_NEXT:
					// tui_data.prev_track = tui_data.current_track;
					free_meta(&tui_data.prev_meta);
					tui_data.prev_meta = tui_data.cur_meta;
					djay_next(&djay_ctx);
					goto BREAK_PLAYBACK;
				case ACTION_PREV:
					// we would have to either remember or reparse metadata
					tui_data.prev_meta = empty_meta;
					djay_prev(&djay_ctx);
					goto BREAK_PLAYBACK;
				case ACTION_STOP:
					stop = true;
					goto BREAK_PLAYBACK;
				default: break;
				}
			}

			tui_draw(&tui_data);

			usleep(50000);
		} // end ui loop

		free_meta(&tui_data.prev_meta);
		tui_data.prev_meta = tui_data.cur_meta;
		djay_next(&djay_ctx);
BREAK_PLAYBACK:
		// cleanup
		pthread_join(dec_thread, NULL);
		decoder_free(&dec_args);
		pthread_join(play_thread, NULL);

		channel_reset(audio_channel);
	} // end playback loop

FAIL:
	ao_shutdown();
	channel_free(audio_channel);

	if (mpris) {
		mpris_free(&mpris_ctx);
	}
	channel_free(event_channel);
	djay_free(&djay_ctx);

	tui_stop();
	free_meta(&tui_data.prev_meta);
	free_meta(&tui_data.cur_meta);

	return 0;
}

