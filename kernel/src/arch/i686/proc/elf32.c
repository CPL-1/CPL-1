#include <arch/i686/proc/elf32.h>

#define EI_DATA 0x05
#define EI_DATA_LE 0x1
#define EI_OSABI 0x07
#define EI_OSABI_SYSV 0x00
#define MACHINE_TYPE_X86 0x03

bool i686_Elf32_HeaderVerifyCallback(struct Elf32_Header *header) {
	if (header->identifier[EI_OSABI] != EI_OSABI_SYSV) {
		return false;
	}
	if (header->machine != MACHINE_TYPE_X86) {
		return false;
	}
	if (header->identifier[EI_DATA] != EI_DATA_LE) {
		return false;
	}
	return true;
}