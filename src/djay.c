#include "djay.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include "comain.h"
#include <unistd.h>

#define PL_SUFFIX ".mus"

// @linux
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

int djay_init(const char* playlist_file, DJayContext* ctx) {
	FILE* file = fopen(playlist_file, "r");
	if (file == NULL) return -1;
#define FAIL CLOSE_FILE

	int length;
	{
		failif (0 > fseek(file, 0, SEEK_END), "Playlist seek failed\n");
		length = ftell(file);
		failif (0 > length, "Playlist tell failed\n");
		failif (0 > fseek(file, 0, SEEK_SET), "Playlist rewind failed\n");
	}

	char* buffer = malloc(length);
	failif (NULL == buffer, "Playlist malloc failed\n");
#undef FAIL
#define FAIL FREE_BUFFER

	failif (length - 1 != fread(buffer, 1, length - 1, file), "Playlist read failed\n");

	int count = 1;
	for (int i = 0; i < length; i++) {
		if (buffer[i] == '\n') count++;
	}
	// failif (0 == count, "Empty or invalid playlist\n");

	buffer[length - 1] = '\0';

	char** playlist = malloc(count * sizeof(*playlist));
	failif(NULL == playlist, "Playlist malloc 2 failed\n");
#undef FAIL
#define FAIL FREE_PLAYLIST

	playlist[0] = buffer;
	int pl_index = 1;
	for (int i = 1; i < length; i++) {
		if (buffer[i - 1] == '\n') {
			playlist[pl_index] = buffer + i;
			buffer[i - 1] = '\0';
			pl_index++;
		}
	}

	fclose(file);

	int path_len = strlen(playlist_file);
	int last_slash = path_len;
	for (int i = path_len - 1; path_len >= 0; i--) {
		if (playlist_file[i] == '/') {
			last_slash = i;
			break;
		}
	}

	// techn. not necessary; can be done above
	int max_name = 0;
	for (int i = 0; i < count; i++) {
		int len = strlen(playlist[i]);
		if (len > max_name) max_name = len;
	}

	char* base_path = malloc(last_slash + 1 + max_name + 1);
	failif (NULL == base_path, "Playlist malloc 3 failed\n");
#undef FAIL
#define FAIL FREE_PATH
	base_path[last_slash] = '/';
	base_path[last_slash + 1] = '\0';
	base_path[last_slash + max_name + 1] = '\0';
	memcpy(base_path, playlist_file, last_slash);

	int hist_len = 10;
	int* history = malloc(hist_len * sizeof(*history));
	failif(NULL == history, "Playlist malloc 4 failed\n");
	for (int i = 0; i < hist_len; i++) {
		int r = random() % count;
		history[i] = r;
	}
#undef FAIL
#define FAIL FREE_HISTORY

	DJayContext result = {
		.playlist = {
			.len = count,
			.cur = 0,
			.entries = playlist,
		},
		.base_path_len = last_slash + 1,
		.current_path = base_path,
		.history = {
			.len = hist_len,
			.cur = 0,
			.entries = history,
		},
	};
	*ctx = result;

	djay_next(ctx);
	return 0;

FREE_HISTORY:
	free(history);
FREE_PATH:
	free(base_path);
FREE_PLAYLIST:
	free(playlist);
FREE_BUFFER:
	free(buffer);
CLOSE_FILE:
	fclose(file);
	return -1;
}

void djay_free(DJayContext* ctx) {
	free(ctx->history.entries);
	free(ctx->current_path);
	free((void*)ctx->playlist.entries[0]);
	free(ctx->playlist.entries);
}

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
			= random() % ctx->playlist.len;
	}

	strcpy(ctx->current_path + ctx->base_path_len,
		ctx->playlist.entries[ctx->playlist.cur]);
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
		ctx->playlist.entries[ctx->playlist.cur]);
}

