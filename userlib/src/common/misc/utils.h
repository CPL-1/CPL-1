#ifndef __UTILS_H_INCLUDED__
#define __UTILS_H_INCLUDED__

#define ALIGN_UP(val, align) ((((val) + (align) - (1)) / (align)) * (align))
#define ALIGN_DOWN(val, align) (((val) / (align)) * (align))
#define ARR_SIZE(val) (sizeof(val) / sizeof(*(val)))

#endif