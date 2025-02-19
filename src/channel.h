#pragma once
#include <stdbool.h>
#include <pthread.h>

#define EFINISHED -1

struct Channel;

typedef struct {
	struct Channel* channel;
	int cursor;
} ChannelConsumer;

typedef struct Channel {
	int capacity;
	void* data;

	int producer_cursor;
	pthread_mutex_t prod_mutex;
	pthread_cond_t producer_cond;
	pthread_mutex_t cons_mutex;
	pthread_cond_t producer_signal;
	bool finished;

	int consumer_count;
	ChannelConsumer* consumers;
} Channel;

Channel* channel_alloc(int capacity, int consumer_count);
void channel_free(Channel* channel);

void channel_write(Channel* channel, void* data, int size);
void channel_finish_writing(Channel* channel);

/// Returns number of elements read.
/// If 0 is returned, channel producer has finished.
int channel_read(ChannelConsumer* consumer, int count, void* dest);
int channel_available(ChannelConsumer* consumer);

/// thread safety: what is that?
void channel_reset(Channel* channel);

