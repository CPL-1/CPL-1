#ifndef __MATH_H_INCLUDED__
#define __MATH_H_INCLUDED__
#include <common/misc/utils.h>

#define MATH_LOG2_ROUNDUP(val)                                                                                         \
	({                                                                                                                 \
		AUTO copy = (val);                                                                                             \
		typeof(copy) current = 1;                                                                                      \
		size_t count = 0;                                                                                              \
		while (current < copy) {                                                                                       \
			current *= 2;                                                                                              \
			count += 1;                                                                                                \
		}                                                                                                              \
		count;                                                                                                         \
	})

#endif