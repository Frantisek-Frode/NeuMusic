#include <stdbool.h>
#define Float float


typedef struct {
	int in_size;
	int out_size;

	Float* matrix; // incl. bias
	Float* values;

	Float* grad_mat;
	Float* grad_val;

	Float* first_moment;
	Float* second_moment;
} NeuLayer;

typedef struct {
	Float learning_rate;
	Float beta1;
	Float gamma1;
	Float beta2;
	Float gamma2;
	Float epsilon;
} AdamCTX;

typedef struct NeuNet_ {
	int in_size; Float* input; Float* in_grad;
	int layer_count; NeuLayer* layers;
	int out_size; Float* output;
	AdamCTX adam;
	void(* grad_cost)(struct NeuNet_* net);
} NeuNet;

NeuNet neunet_alloc(
	int layers, int* layer_sizes,
	Float learning_rate, Float beta1, Float beta2,
	void(* grad_cost)(NeuNet* net)
);
void neunet_free(NeuNet* net);

void neunet_compute(NeuNet* net);
void neunet_backpropagate(NeuNet* net);
void neunet_step(NeuNet* net, Float grad_mult);

void neunet_dump(NeuNet* net);
int neunet_save(NeuNet* net, const char* file, bool save_opt_state);
int neunet_load(
	const char* file,
	Float def_learning_rate, Float def_beta1, Float def_beta2,
	void(* grad_cost)(NeuNet* net),
	NeuNet* dest
);


