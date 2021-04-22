#ifndef __CPL1_LIBC_SYS_TIME_H_INCLUDED__
#define __CPL1_LIBC_SYS_TIME_H_INCLUDED__

#include <sys/types.h>

struct timeval {
	time_t tv_sec;
	suseconds_t tv_usec;
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

#endif
