#include "neunet.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>


Float* vec_alloc(size_t size) {
	void* result = malloc(size * sizeof(Float));
	if (result == NULL) {
		fprintf(stderr, "[CRIT] Malloc failed\n");
		exit(-1);
	}

	return result;
}

void layer_alloc(NeuLayer* result, int in_size, int out_size) {
	NeuLayer layer = {
		.in_size = in_size,
		.out_size = out_size,
		.matrix = vec_alloc((in_size + 1) * out_size),
		.values = vec_alloc(out_size),
		.grad_mat = vec_alloc((in_size + 1) * out_size),
		.grad_val = vec_alloc(out_size),
		.first_moment = vec_alloc((in_size + 1) * out_size),
		.second_moment = vec_alloc((in_size + 1) * out_size)
	};

	for (int i = 0; i < (in_size + 1) * out_size; i++) {
		layer.matrix[i] = (Float)rand() / (Float)RAND_MAX - .5;
	}
	memset(layer.first_moment, 0, (in_size + 1) * out_size * sizeof(Float));
	memset(layer.second_moment, 0, (in_size + 1) * out_size * sizeof(Float));

	*result = layer;
}

void layer_free(NeuLayer* layer) {
	if (layer == NULL) return;
	free(layer->matrix);
	free(layer->values);
	free(layer->grad_mat);
	free(layer->grad_val);
	free(layer->first_moment);
	free(layer->second_moment);
}

NeuNet neunet_alloc(
	int layers, int* layer_sizes,
	Float learning_rate, Float beta1, Float beta2,
	void(* grad_cost)(NeuNet* net)
) {
	if (layers < 1) {
		fprintf(stderr, "[CRIT] At least one layer is required\n");
		exit(-1);
	}

	NeuNet result = {
		.layer_count = layers - 1,
		.layers = malloc((layers - 1) * sizeof(NeuLayer)),
		.in_size = layer_sizes[0],
		.out_size = layer_sizes[layers - 1],
		.input = vec_alloc(layer_sizes[0]),
		.in_grad = vec_alloc(layer_sizes[0]),
		.adam = {
			.learning_rate = learning_rate,
			.beta1 = beta1,
			.beta2 = beta2,
			.epsilon = 1e-8,
			.gamma1 = 1,	// (1-b1^i)/(1-b1)
			.gamma2 = 1,
		},
		.grad_cost = grad_cost
	};

	if (result.layers == NULL) {
		fprintf(stderr, "[CRIT] Malloc failed\n");
		exit(-1);
	}

	for (int i = 0; i < layers - 1; i++) {
		layer_alloc(&result.layers[i], layer_sizes[i], layer_sizes[i+1]);
	}
	result.output = result.layers[layers - 2].values;

	return result;
}

void neunet_free(NeuNet* net) {
	if (net == NULL) return;
	free(net->input);
	free(net->in_grad);

	for (int i = 0; i < net->layer_count; i++) {
		layer_free(&net->layers[i]);
	}

	free(net->layers);
}

void neunet_dump(NeuNet* net) {
	for (int lay = 0; lay < net->layer_count; lay++) {
		NeuLayer* layer = &net->layers[lay];

		for (int i = 0; i < (layer->in_size + 1) * layer->out_size; i++) {
			fprintf(stderr, "%f ", layer->matrix[i]);
		}
		fprintf(stderr, "\n");
	}
}

Float activation(Float x) {
	return tanh(x);
	// return log(1 + exp(x)) - 1 + .1 * x;
}

Float d_activation(Float x) {
	Float t = tanh(x);
	return 1 - t * t;
	// return 1 / (1 + exp(-x)) + .1;
}

void neunet_compute(NeuNet* net) {
#define A(i, j) layer->matrix[(layer->in_size + 1) * (i) + (j) + 1]
#define b(i) layer->matrix[(layer->in_size + 1) * (i)]

	Float* input = net->input;

	for (int lay = 0; lay < net->layer_count; lay++) {
		NeuLayer* layer = &net->layers[lay];
		for (int row = 0; row < layer->out_size; row++) {
			Float val = b(row);
			for (int col = 0; col < layer->in_size; col++) {
				val += A(row, col) * input[col];
			}
			layer->values[row] = activation(val);
		}
		input = layer->values;
	}
#undef A
#undef b
}

void neunet_backpropagate(NeuNet* net) {
#define A(i, j) layer->matrix[(layer->in_size + 1) * (i) + (j) + 1]
#define dA(i, j) layer->grad_mat[(layer->in_size + 1) * (i) + (j) + 1]
#define db(i) layer->grad_mat[(layer->in_size + 1) * (i)]

	net->grad_cost(net);
	for (int lay = net->layer_count - 1; lay >= 0; lay--) {
		NeuLayer* layer = &net->layers[lay];

		Float* prev_values;
		if (lay > 0) {
			prev_values = net->layers[lay - 1].values;
		} else {
			prev_values = net->input;
		}

		for (int row = 0; row < layer->out_size; row++) {
			layer->grad_val[row] *= d_activation(layer->grad_val[row]);
			db(row) = layer->grad_val[row];
			for (int col = 0; col < layer->in_size; col++) {
				dA(row, col) = layer->grad_val[row] * prev_values[col];
			}
		}

		Float* prev_grad_val;
		if (lay > 0) {
			prev_grad_val = net->layers[lay - 1].grad_val;
		} else {
			prev_grad_val = net->in_grad;
		}

		for (int col = 0; col < layer->in_size; col++) {
			prev_grad_val[col] = 0;
			for (int row = 0; row < layer->out_size; row++) {
				prev_grad_val[col] += layer->grad_val[row] * A(row, col);
			}
		}
	}

#undef A
#undef dA
#undef db
}

void neunet_step(NeuNet* net, Float grad_mult) {
#define ind(i, j) ((layer->in_size + 1) * (i) + (j) + 1)

	AdamCTX* adam = &net->adam;
	for (int lay = 0; lay < net->layer_count; lay++) {
		NeuLayer* layer = &net->layers[lay];

		for (int row = 0; row < layer->out_size; row++) {
			for (int col = -1; col < layer->in_size; col++) {
				int i = ind(row, col);
				layer->grad_mat[i] *= grad_mult;
				layer->first_moment[i] = adam->beta1 * layer->first_moment[i] + (1 - adam->beta1) * layer->grad_mat[i];
				layer->second_moment[i] = adam->beta2 * layer->second_moment[i] + (1 - adam->beta2) * layer->grad_mat[i] * layer->grad_mat[i];
				Float first_hat = layer->first_moment[i] / ((1 - adam->beta1) * adam->gamma1);
				Float second_hat = layer->second_moment[i] / ((1 - adam->beta2) * adam->gamma2);
				layer->matrix[i] -= adam->learning_rate * first_hat / (sqrt(second_hat) + adam->epsilon);
			}
		}
	}

	adam->gamma1 = adam->beta1 * (1 + adam->gamma1);
	adam->gamma2 = adam->beta2 * (1 + adam->gamma2);

#undef ind
}

const uint16_t FORMAT_VERSION = 0x0001;
int neunet_save(NeuNet* net, const char* file, bool opt_state) {
	FILE* handle = fopen(file, "wb");
	if (handle == NULL) {
		fprintf(stderr, "[ERR] Failed to open file '%s' for writing\n", file);
		return -1;
	}

#define checked_write(data, n) do {\
	int written = fwrite((data), sizeof(*(data)), (n), handle);\
	if (written < (n)) {\
		fprintf(stderr, "[ERR] Wrote less data then expected [%d < %d]\n", written, (n));\
		fclose(handle);\
		return -1;\
	}\
} while(0);

	checked_write(&FORMAT_VERSION, 1);
	checked_write(&opt_state, 1);

	checked_write(&net->layer_count, 1);
	int in_size = net->in_size;
	checked_write(&in_size, 1);
	for (int lay = 0; lay < net->layer_count; lay++) {
		checked_write(&net->layers[lay].out_size, 1);
	}

	for (int lay = 0; lay < net->layer_count; lay++) {
		int out_size = net->layers[lay].out_size;
		checked_write(net->layers[lay].matrix, (in_size + 1) * out_size);

		if (opt_state) {
			checked_write(net->layers[lay].grad_mat, (in_size + 1) * out_size);
			checked_write(net->layers[lay].first_moment, (in_size + 1) * out_size);
			checked_write(net->layers[lay].second_moment, (in_size + 1) * out_size);
		}

		in_size = out_size;
	}

	if (opt_state) {
		checked_write(&net->adam, 1);
	}

	fclose(handle);
	return 0;

#undef checked_write
}

int neunet_load(
	const char* file,
	Float learning_rate, Float beta1, Float beta2,
	void(* grad_cost)(NeuNet* net),
	NeuNet* result
) {
	FILE* handle = fopen(file, "rb");
	if (handle == NULL) {
		fprintf(stderr, "[ERR] Failed to open file '%s' for reading\n", file);
		return -1;
	}

#define checked_read(dest, n) do {\
	int read = fread((dest), sizeof(*(dest)), (n), handle);\
	if (read < (n)) {\
		fprintf(stderr, "[ERR] Read less data then expected [%d < %d]\n", read, (n));\
		fclose(handle);\
		return -1;\
	}\
} while(0);

	uint16_t version;
	checked_read(&version, 1);
	if (version > 0xff) {
		fprintf(stderr, "[ERR] Unsupported file endiannes\n");
		fclose(handle);
		return -1;
	} else if (version != 1) {
		fprintf(stderr, "[ERR] Unsupported protocol version: %d\n", version);
		fclose(handle);
		return -1;
	}

	bool opt_state;
	checked_read(&opt_state, 1);

	int layer_count;
	checked_read(&layer_count, 1);

	int* layer_sizes = malloc((layer_count + 1) * sizeof(*layer_sizes));
	checked_read(layer_sizes, layer_count + 1);

	*result = neunet_alloc(layer_count + 1, layer_sizes, learning_rate, beta1, beta2, grad_cost);
	free(layer_sizes);

	int in_size = result->in_size;
	for (int lay = 0; lay < result->layer_count; lay++) {
		int out_size = result->layers[lay].out_size;
		checked_read(result->layers[lay].matrix, (in_size + 1) * out_size);

		if (opt_state) {
			checked_read(result->layers[lay].grad_mat, (in_size + 1) * out_size);
			checked_read(result->layers[lay].first_moment, (in_size + 1) * out_size);
			checked_read(result->layers[lay].second_moment, (in_size + 1) * out_size);
		}

		in_size = out_size;
	}

	if (opt_state) {
		checked_read(&result->adam, 1);
	}

	fclose(handle);
	return 0;
}


