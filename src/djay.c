#include "djay.h"
#include "comain.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define PL_VERSION "1\n"


float frand(float min, float max) {
	long val = random();
	double val2 = (double)val / (double)RAND_MAX;

	return val2 * (max - min) + min;
}

int read_entry(FILE* file, DJEntry* data, int* path_length) {
	DJEntry entry;
	char buffer[1000];

	int read = fscanf(file, "[%a;%a,%a,%a,%a,%a]%999[^\n]\n",
		&entry.confidence,
		&entry.characteristics[0],
		&entry.characteristics[1],
		&entry.characteristics[2],
		&entry.characteristics[3],
		&entry.characteristics[4],
		buffer
	);

	if (read != 2 + CHAR_COUNT) {
		read = fscanf(file, "%999[^\n]\n", buffer);
		if (read <= 0) return 1; // EOF

		entry.confidence = 0;
		for (int i = 0; i < CHAR_COUNT; i++) {
			entry.characteristics[i] = frand(-.1, .1);
		}
	}

	int path_len = strlen(buffer) + 1;
	entry.path = malloc(path_len);
	if (entry.path == NULL) {
		fprintf(stderr, "OOM\n");
		return -1;
	}
	memcpy((char*)entry.path, buffer, path_len);

	entry.times_played = 0;

	*path_length = path_len;
	*data = entry;
	return 0;
}

void free_entry(DJEntry* entry) {
	free((char*)entry->path);
}


#define PL_SUFFIX ".mus"
const char* djay_create_playlist(const char* dir_name, const char* _Nullable filename) {
	if (NULL == filename) {
		filename = "playlist"PL_SUFFIX;
	}

	int dir_len = strlen(dir_name);
	if (dir_len > 0 && dir_name[dir_len - 1] == '/') dir_len--;

	char* fullname = calloc(dir_len + 1 + strlen(filename) + 1, 1);
	if (fullname == NULL) { fprintf(stderr, "OOM"); goto FAIL; }
	strcpy(fullname, dir_name);
	fullname[dir_len] = '/';
	strcpy(fullname + dir_len + 1, filename);

	DIR* dir = opendir(dir_name);
	if (NULL == dir) { fprintf(stderr, "Could not open directory '%s'\n", dir_name); goto FAIL; }

	FILE* file = fopen(fullname, "w");
	if (NULL == file) { fprintf(stderr, "Could not open playlist file '%s'\n", fullname); closedir(dir); goto FAIL; }

// @linux
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type != DT_REG) continue;

		int name_len = strlen(entry->d_name);
		if (name_len >= strlen(PL_SUFFIX)
		&& 0==strcmp(entry->d_name + name_len - strlen(PL_SUFFIX), PL_SUFFIX))
			continue;

		fprintf(file, "%s\n", entry->d_name);
	}

	fclose(file);
	closedir(dir);

	return fullname;
FAIL:
	free(fullname);
	return NULL;
}


// inputs: chracteristics, #played, mood, time, day
int neu_layers[] = {
	CHAR_COUNT + MOOD_COUNT + 3,
	10, 5, 3,
	1
};
// since the last layer has only one neuron, the gradient can
// be pre-computed, up to scaling
void neu_grad(NeuNet* net) {
	net->layers[net->layer_count - 1].grad_val[0] = 1;
}


int djay_init(const char* playlist_file, DJayContext* ctx) {
	// load playlist entries
	FILE* file = fopen(playlist_file, "r");
	if (file == NULL) return -1;
#define FAIL CLOSE_FILE

	int entry_count = 0;
	int entry_cap = 200;
	DJEntry* entries = malloc(entry_cap * sizeof(*entries));
	failif(NULL == entries, "Playlist malloc failed\n");
#undef FAIL
#define FAIL FREE_ENTIRES

	int max_name = 0;
	for (int i = 0, failed = 0; !failed; i++) {
		if (i == entry_cap) {
			entry_cap *= 2;
			entries = realloc(entries, entry_cap * sizeof(*entries));
			failif(NULL == entries, "Playlist malloc failed\n");
		}

		int name_len;
		failed = read_entry(file, &entries[i], &name_len);
		if (failed < 0) goto FAIL;
		if (max_name < name_len) max_name = name_len;
		entry_count = i;
	}
	entries = realloc(entries, entry_count * sizeof(*entries));
	entry_cap = entry_count;

	fclose(file);


	// prepare absolute-ish path
	int path_len = strlen(playlist_file);
	int last_slash = path_len;
	for (int i = path_len - 1; path_len >= 0; i--) {
		if (playlist_file[i] == '/') {
			last_slash = i;
			break;
		}
	}

	char* base_path = malloc(last_slash + 1 + max_name + 1);
	failif (NULL == base_path, "Playlist malloc 3 failed\n");
#undef FAIL
#define FAIL FREE_PATH
	base_path[last_slash] = '/';
	base_path[last_slash + 1] = '\0';
	base_path[last_slash + max_name + 1] = '\0';
	memcpy(base_path, playlist_file, last_slash);


	// fill history
	int hist_len = 10;
	int* history = malloc(hist_len * sizeof(*history));
	failif(NULL == history, "Playlist malloc 4 failed\n");
	for (int i = 0; i < hist_len; i++) {
		int r = random() % entry_count;
		history[i] = r;
	}
#undef FAIL
#define FAIL FREE_HISTORY


	// set results and select track
	DJayContext result = {
		.playlist = {
			.len = entry_count,
			.cur = 0,
			.entries = entries,
		},
		.base_path_len = last_slash + 1,
		.current_path = base_path,
		.history = {
			.len = hist_len,
			.cur = 0,
			.entries = history,
		},
		// TODO: load neunet
		.neu = neunet_alloc(
			sizeof(neu_layers) / sizeof(*neu_layers), neu_layers,
			.1, .7, .99,
			neu_grad
		),
		.mood_conf = 0,
	};

	memset(result.mood, 0, sizeof(result.mood));
	*ctx = result;

	djay_next(ctx);
	return 0;


	// error handling
FREE_HISTORY:
	free(history);
FREE_PATH:
	free(base_path);
FREE_ENTIRES:
	for (int j = 0; j < entry_count; j++) {
		free_entry(&entries[j]);
	}
	free(entries);
CLOSE_FILE:
	fclose(file);
	return -1;
}

void djay_free(DJayContext* ctx) {
	free(ctx->history.entries);
	free(ctx->current_path);
	for (int j = 0; j < ctx->playlist.len; j++) {
		free_entry(&ctx->playlist.entries[j]);
	}
	free(ctx->playlist.entries);
	neunet_free(&ctx->neu);
}

// implementation bellow
int select_next(DJayContext* ctx);

void djay_next(DJayContext* ctx) {
	int last = ctx->history.cur;

	ctx->history.cur++;
	if (ctx->history.cur >= ctx->history.len)
		ctx->history.cur -= ctx->history.len;

	if (ctx->history.last != last) {
		ctx->playlist.cur = ctx->history.entries[ctx->history.cur];
	} else {
		ctx->history.last = ctx->history.cur;

		ctx->playlist.cur
			= ctx->history.entries[ctx->history.cur]
			= select_next(ctx);
	}

	strcpy(ctx->current_path + ctx->base_path_len,
		ctx->playlist.entries[ctx->playlist.cur].path);
}

void djay_prev(DJayContext* ctx) {
	int prev = ctx->history.cur - 1;
	if (prev < 0) prev += ctx->history.len;

	if (prev == ctx->history.last) {
		// going further would make little sense
		// TODO: play warning sound
		return;
	}

	ctx->history.cur = prev;

	ctx->playlist.cur = ctx->history.entries[ctx->history.cur];
	strcpy(ctx->current_path + ctx->base_path_len,
		ctx->playlist.entries[ctx->playlist.cur].path);
}


void feed_ctx(NeuNet* net, DJayContext* ctx) {
	memcpy(&net->input[CHAR_COUNT + 3], ctx->mood, sizeof(ctx->mood));

	time_t now = time(NULL);
	struct tm tm = *localtime(&now);
	float tod = tm.tm_hour * 60 + tm.tm_min;
	float dow = tm.tm_wday - 1;
	if (dow < 0) dow = 6;

	// All days shall henceforth have 24 hours. No more, no less.
	net->input[CHAR_COUNT + 1] = tod / (24 * 60);
	net->input[CHAR_COUNT + 2] = dow / 7;
}

void feed_entry(NeuNet* net, const DJEntry* entry) {
	memcpy(net->input, entry->characteristics, sizeof(entry->characteristics));
	net->input[CHAR_COUNT] = entry->times_played;
}

int select_next(DJayContext* ctx) {
	float dist = frand(0.1, ctx->playlist.len);
	int eid = ctx->playlist.cur;

	NeuNet* neu = &ctx->neu;
	feed_ctx(neu, ctx);

	while (dist > 0) {
		eid++;
		if (eid >= ctx->playlist.len) eid = 0;

		feed_entry(neu, &ctx->playlist.entries[eid]);

		neunet_compute(neu);

		Float dec = (neu->output[0] + 1.1) * 5;
		dist -= dec;
	}

	ctx->playlist.entries[eid].times_played++;

	return eid;
}

void djay_rate(DJayContext* ctx, Float rating) {
	NeuNet* neu = &ctx->neu;
	feed_ctx(neu, ctx);

	int eid = ctx->playlist.cur;
	DJEntry* entry = &ctx->playlist.entries[eid];
	feed_entry(neu, entry);

	neunet_compute(neu);
	neunet_backpropagate(neu);

	neunet_step(neu, -rating);

	ctx->mood_conf += rating;
	Float mult = -rating * .01 * exp(-ctx->mood_conf);
	for (int i = 0; i < MOOD_COUNT; i++) {
		ctx->mood[i] += mult * ctx->neu.in_grad[CHAR_COUNT + 1 + i];
	}

	entry->confidence += rating;
	mult = -rating * .01 * exp(-entry->confidence);
	for (int i = 0; i < CHAR_COUNT; i++) {
		entry->characteristics[i] += mult * ctx->neu.in_grad[i];
	}
}

