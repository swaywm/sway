#include "util.h"

int wrap(int i, int max) {
	return ((i % max) + max) % max;
}
