#include <common/lib/time.h>

time_t Time_GetYDays(time_t days, time_t months) {
	static time_t cumulativeDaysTable[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304};
	return (days - 1) + cumulativeDaysTable[(months - 1)];
}

// https://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap04.html#tag_04_14
time_t Time_UTCToUnixTimestamp(time_t seconds, time_t minutes, time_t hours, time_t days, time_t months, time_t years) {
	time_t ydays = Time_GetYDays(days, months);
	return seconds + minutes * 60 + hours * 3600 + ydays * 86400 + (years - 70) * 31536000 +
		   ((years - 69) / 4) * 86400 - ((years - 1) / 100) * 86400 + ((years + 299) / 400) * 86400;
}
