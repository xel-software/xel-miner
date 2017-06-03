/*
* Copyright 2016 sprocket
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#ifndef ELASTICPLFUNCTIONS_H_
#define ELASTICPLFUNCTIONS_H_

#include <stdint.h>

#ifndef MAX
	#define MAX(a,b) ((a) > (b) ? a : b)
	#define MIN(a,b) ((a) < (b) ? a : b)
#endif

extern int32_t gcd(int32_t	a, int32_t b);

#endif // ELASTICPLFUNCTIONS_H_