#ifndef __MATH_H_INCLUDED__
#define __MATH_H_INCLUDED__
#include <common/misc/utils.h>

#define MATH_LOG2_ROUNDUP(_val)                                                                                        \
	({                                                                                                                 \
		AUTO _copy = (_val);                                                                                           \
		size_t _current = 1;                                                                                           \
		size_t _count = 0;                                                                                             \
		while (_current < _copy) {                                                                                     \
			_current *= 2;                                                                                             \
			_count += 1;                                                                                               \
		}                                                                                                              \
		_count;                                                                                                        \
	})

#endif
