#include "channel.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define plusmod(var, a, b, mod) var = a + b; if (var >= mod) var -= mod;

#define alloc(var, length) var = malloc((length) * sizeof(*var));\
	if (var == NULL) {\
		fprintf(stderr, "Malloc failed\n"); ERR }

#define salloc(var, length, size) var = malloc((length) * (size));\
	if (var == NULL) {\
		fprintf(stderr, "Malloc failed\n"); ERR }


int ring_capacity(int writer, int reader, int capacity) {
	if (writer < reader) return reader - writer - 1;
	else return capacity - 1 + reader - writer;
}

int ring_available(int writer, int reader, int capacity) {
	if (reader <= writer) return writer - reader;
	else return writer - reader + capacity;
}

void copy_to_ring(void* ring, int capacity, int writer, void* src, int count) {
	if (writer + count <= capacity) {
		memcpy((char*)ring + writer, src, count);
	} else {
		int right = capacity - writer;
		memcpy((char*)ring + writer, src, right);
		memcpy(ring, (char*)src + right, count - right);
	}
}

void copy_from_ring(void* ring, int capacity, int reader, void* dest, int count) {
	if (reader + count <= capacity) {
		memcpy(dest, (char*)ring + reader, count);
	} else {
		int right = capacity - reader;
		memcpy(dest, (char*)ring + reader, right);
		memcpy((char*)dest + right, ring, count - right);
	}
}


Channel* channel_alloc(int cap, int consumers) {
#define ERR \
	if (ret) {\
		free(ret->data);\
		free(ret->consumers);\
	}\
	free(ret);\
	return NULL;

	if (cap <= 0 || consumers <= 0) {
		fprintf(stderr, "Channel capacity and consumer count must be positive\n");
		return NULL;
	}


	Channel* alloc(ret, 1);
	Channel result = {
		.capacity = cap,
		.data = NULL,
		.producer_cursor = 0,
		.finished = false,
		.consumer_count = consumers,
		.consumers = NULL,
	};

	salloc(result.data, result.capacity, 1);
	alloc(result.consumers, result.consumer_count);

	pthread_mutex_init(&result.prod_mutex, NULL);
	pthread_mutex_init(&result.cons_mutex, NULL);

	pthread_cond_init(&result.producer_cond, NULL);
	pthread_cond_init(&result.producer_signal, NULL);

	for (int c = 0; c < consumers; c++) {
		ChannelConsumer cons = {
			.channel = ret,
			.cursor = 0,
		};
		result.consumers[c] = cons;
	}

	*ret = result;
	return ret;
#undef ERR
}

void channel_free(Channel* channel) {
	if (!channel) return;

	free(channel->data);
	free(channel->consumers);

	pthread_mutex_destroy(&channel->prod_mutex);
	pthread_mutex_destroy(&channel->cons_mutex);

	pthread_cond_destroy(&channel->producer_cond);
	pthread_cond_destroy(&channel->producer_signal);

	free(channel);
}

void channel_write(Channel* channel, void* data, int size) {
	int next = channel->producer_cursor + size;
	if (next >= channel->capacity) next -= channel->capacity;

	pthread_mutex_lock(&channel->prod_mutex);
	for (int i = 0; i < channel->consumer_count; i++) {
		int space = ring_capacity(channel->producer_cursor, channel->consumers[i].cursor, channel->capacity);

		while (space < size) {
			pthread_cond_wait(&channel->producer_cond, &channel->prod_mutex);
			space = ring_capacity(channel->producer_cursor, channel->consumers[i].cursor, channel->capacity);
		}
	}
	pthread_mutex_unlock(&channel->prod_mutex);

	copy_to_ring(channel->data, channel->capacity, channel->producer_cursor, data, size);
	channel->producer_cursor = next;

	pthread_mutex_lock(&channel->cons_mutex);
	pthread_cond_broadcast(&channel->producer_signal);
	pthread_mutex_unlock(&channel->cons_mutex);
}

void channel_finish_writing(Channel* channel) {
	channel->finished = true;

	pthread_mutex_lock(&channel->cons_mutex);
	pthread_cond_broadcast(&channel->producer_signal);
	pthread_mutex_unlock(&channel->cons_mutex);
}

int channel_read(ChannelConsumer* consumer, int count, void* result) {
	Channel* channel = consumer->channel;
	int available;

	pthread_mutex_lock(&channel->cons_mutex);
	for (;;) {
		int ready = ring_available(channel->producer_cursor, consumer->cursor, channel->capacity);
		if (ready >= count) {
			available = count;
			break;
		} else if (channel->finished) {
			available = ready;
			break;
		} else {
			pthread_cond_wait(&channel->producer_signal, &channel->cons_mutex);
		}
	}
	pthread_mutex_unlock(&channel->cons_mutex);

	if (available == 0) return 0;
	copy_from_ring(channel->data, channel->capacity, consumer->cursor, result, available);

	consumer->cursor += available;
	if (consumer->cursor >= channel->capacity) consumer->cursor -= channel->capacity;

	if (!channel->finished) {
		pthread_mutex_lock(&channel->prod_mutex);
		pthread_cond_signal(&channel->producer_cond);
		pthread_mutex_unlock(&channel->prod_mutex);
	}

	return available;
}

int channel_available(ChannelConsumer* consumer) {
	Channel* channel = consumer->channel;

	return ring_available(channel->producer_cursor, consumer->cursor, channel->capacity);
}

