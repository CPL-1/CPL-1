#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/log.h>
#include <sys/time.h>
#include <unistd.h>

void Date_PrintVersion() {
	printf("echo. Copyright (C) 2021 Zamiatin Iurii and CPL-1 contributors\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY; for details type \"echo --license\"\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; type \"echo --license\" for details.\n");
}

void Date_PrintHelp() {
	printf("usage: date\n");
}

void Date_PrintLicense() {
	char buf[40000];
	int licenseFd = open("/etc/src/COPYING", O_RDONLY);
	if (licenseFd < 0) {
		Log_ErrorMsg("\"date\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	int bytes = read(licenseFd, buf, 40000);
	if (bytes < 0) {
		Log_ErrorMsg("\"date\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	buf[bytes] = '\0';
	printf("%s\n", buf);
}

void Date_PadToTwoDigits(int num, char *buf) {
	if (num < 9) {
		buf[0] = '0';
		buf[1] = num + '0';
	} else {
		buf[0] = (num / 10) + '0';
		buf[1] = (num % 10) + '0';
	}
	buf[2] = '\0';
}

int main(int argc, char const *argv[]) {
	if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		Date_PrintVersion();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		Date_PrintHelp();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--license") == 0)) {
		Date_PrintLicense();
		return 0;
	}
	struct timeval val;
	if (gettimeofday(&val, NULL) < 0) {
		Log_ErrorMsg("\"date\" Utility", "Failed to get current date time with gettimeofday() system call");
	}
	time_t unixTime = val.tv_sec;

	time_t seconds = unixTime % 60;
	time_t minutes = (unixTime / 60) % 60;
	time_t hours = (unixTime / (60 * 60)) % 24;

	time_t dayID = unixTime / (24 * 60 * 60);
	time_t weekday = (dayID >= -4 ? (dayID + 4) % 7 : (dayID + 5) % 7 + 6);

	// see https://stackoverflow.com/questions/7960318/math-to-convert-seconds-since-1970-into-date-and-vice-versa
	// civil_from_days. If you think I know what's going on here, then you are wrong
	dayID += 719468;

	static const time_t daysInEra = 146097;

	time_t era = (dayID >= 0 ? dayID : dayID - (daysInEra - 1)) / daysInEra;
	time_t dayOfEra = dayID - era * daysInEra;
	// some accounting for leap years here
	time_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
	time_t year = yearOfEra + 400 * era;
	time_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
	time_t monthEncoded = (5 * dayOfYear + 2) / 153;
	time_t day = dayOfYear - (153 * monthEncoded + 2) / 5 + 1;
	time_t month = monthEncoded + (monthEncoded < 10 ? 3 : -9);
	if (month <= 2) {
		year += 1;
	}

	static const char *weekdayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
	static const char *monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
									   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	char hoursBuf[3], minutesBuf[3], secondsBuf[3];
	Date_PadToTwoDigits(seconds, secondsBuf);
	Date_PadToTwoDigits(minutes, minutesBuf);
	Date_PadToTwoDigits(hours, hoursBuf);
	printf("%s %d %s %d %s:%s:%s UTC\n", weekdayNames[weekday - 1], (int)day, monthNames[month - 1], (int)year,
		   hoursBuf, minutesBuf, secondsBuf);

	return 0;
}
