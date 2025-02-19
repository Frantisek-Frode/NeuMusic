#pragma once
#define failif(cond, ...) if (cond) { fprintf(stderr, __VA_ARGS__); goto FAIL; }

