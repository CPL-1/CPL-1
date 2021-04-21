#ifndef __TIME_H_INCLUDED__
#define __TIME_H_INCLUDED__

#include <common/misc/types.h>

typedef int64_t time_t;
typedef uint64_t suseconds_t;

struct timeval {
	time_t tv_sec;
	suseconds_t tv_usec;
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

// year is since 1900
time_t Time_UTCToUnixTimestamp(time_t seconds, time_t minutes, time_t hours, time_t days, time_t months, time_t years);

#endif
