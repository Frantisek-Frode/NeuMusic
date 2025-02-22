#include "comain.h"

void free_meta(Metadata* meta) {
	free((char*)meta->author);
	free((char*)meta->title);
}

