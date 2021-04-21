#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/rtc.h>
#include <common/lib/kmsg.h>
#include <common/lib/time.h>
#include <common/misc/utils.h>
#include <hal/drivers/time.h>
#include <hal/proc/intlevel.h>

uint8_t CMOS_ReadByte(uint8_t offset) {
	uint8_t result = 0;
	int level = HAL_InterruptLevel_Elevate();
	if (level == 0) {
		i686_Ports_WriteByte(0x70, offset);
		result = i686_Ports_ReadByte(0x71);
	}
	HAL_InterruptLevel_Recover(level);
	return result;
}

struct RTC_TimeRegs {
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t day;
	uint8_t month;
	uint8_t year;
};

void RTC_TryReadingEncodedTimeRegs(struct RTC_TimeRegs *regs) {
	regs->second = CMOS_ReadByte(0x00);
	regs->minute = CMOS_ReadByte(0x02);
	regs->hour = CMOS_ReadByte(0x04);
	regs->day = CMOS_ReadByte(0x07);
	regs->month = CMOS_ReadByte(0x08);
	regs->year = CMOS_ReadByte(0x09);
}

bool RTC_CompareForEquality(struct RTC_TimeRegs *set1, struct RTC_TimeRegs *set2) {
	return (set1->second == set2->second) && (set1->minute == set2->minute) && (set1->hour == set2->hour) &&
		   (set1->day == set2->day) && (set1->month == set2->month) && (set1->year == set2->year);
}

void RTC_ReadEncodedTimeRegs(struct RTC_TimeRegs *regs) {
	while (true) {
		struct RTC_TimeRegs set1, set2;
		RTC_TryReadingEncodedTimeRegs(&set1);
		RTC_TryReadingEncodedTimeRegs(&set2);
		if (RTC_CompareForEquality(&set1, &set2)) {
			*regs = set1;
			return;
		}
	}
}

void RTC_ReadTimeRegs(struct RTC_TimeRegs *regs) {
	RTC_ReadEncodedTimeRegs(regs);
	uint8_t attributes = CMOS_ReadByte(0x0B);
	// BCD to binary if needed
	if ((attributes & 0x04) == 0) {
		regs->second = (regs->second & 0x0F) + ((regs->second / 16) * 10);
		regs->minute = (regs->minute & 0x0F) + ((regs->minute / 16) * 10);
		regs->hour = ((regs->hour & 0x0F) + (((regs->hour & 0x70) / 16) * 10)) | (regs->hour & 0x80);
		regs->day = (regs->day & 0x0F) + ((regs->day / 16) * 10);
		regs->month = (regs->month & 0x0F) + ((regs->month / 16) * 10);
		regs->year = (regs->year & 0x0F) + ((regs->year / 16) * 10);
	}
	// 12 hour clock to 24 hour clock if needed
	if (((attributes & 0x02) == 0) && ((regs->hour & 0x80) != 0)) {
		regs->hour = ((regs->hour & 0x7f) + 12) % 24;
	}
}

time_t HAL_Time_GetUnixTime() {
	struct RTC_TimeRegs time;
	RTC_ReadTimeRegs(&time);
	// assuming that we live in 21st century
	return Time_UTCToUnixTimestamp(time.second, time.minute, time.hour, time.day, time.month, time.year + 100);
}

void RTC_Init() {
	KernelLog_InitDoneMsg("Real Time Clock driver");
}
