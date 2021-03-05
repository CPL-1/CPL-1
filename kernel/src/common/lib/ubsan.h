#ifndef __UBSAH_H_INCLUDED__
#define __UBSAN_H_INCLUDED__

#include <stdint.h>

struct UBSan_Type {
	uint16_t kind;
	uint16_t info;
	char name[];
};

struct UBSan_SourceLocation {
	const char *file;
	uint32_t line;
	uint32_t column;
};

struct UBSan_MismatchData {
	struct UBSan_SourceLocation location;
	struct UBSan_Type *type;
	uint8_t align;
	uint8_t kind;
};

struct UBSan_OverflowData {
	struct UBSan_SourceLocation location;
	struct UBSan_Type *type;
};

struct UBSan_ShiftOutOfBoundsData {
	struct UBSan_SourceLocation location;
	struct UBSan_Type *lhsType;
	struct UBSan_Type *rh;
};

struct UBSan_OutOfBoundsData {
	struct UBSan_SourceLocation location;
	struct UBSan_Type *arrayType;
	struct UBSan_Type *indexType;
};

#endif
