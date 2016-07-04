#ifndef _SWAY_RESIZE_H
#define _SWAY_RESIZE_H
#include <stdbool.h>

enum resize_dim_types {
	RESIZE_DIM_PX,
	RESIZE_DIM_PPT,
	RESIZE_DIM_DEFAULT,
};

bool set_size(int dimension, bool use_width);
bool resize(int dimension, bool use_width, enum resize_dim_types dim_type);

#endif
