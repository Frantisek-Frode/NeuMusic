#pragma once
#define _Nullable

#define CHAR_COUNT 5
typedef struct {
	float confidence;
	float characteristics[CHAR_COUNT];

	int index;
	const char* path;

	int times_played;
} DJEntry;

#define MOOD_COUNT 3
typedef struct {
	struct {
		int len;
		int cur;
		DJEntry* entries;
	} playlist;

	float mood[MOOD_COUNT];

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

