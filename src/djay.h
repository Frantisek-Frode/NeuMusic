#pragma once
#define _Nullable

typedef struct {
	struct {
		int len;
		int cur;
		char** entries;
	} playlist;

	int base_path_len;
	char* current_path;

	struct {
		int len;
		int cur;
		int last;
		int* entries;
	} history;
} DJayContext;

/// @returns path of the new playlist file or NULL if it fails.
/// Retutrned value must be `free()` d
const char* djay_create_playlist(const char* dir, const char* _Nullable filename);

int djay_init(const char* playlist_file, DJayContext* ctx);
void djay_free(DJayContext* ctx);
void djay_next(DJayContext* ctx);
void djay_prev(DJayContext* ctx);

