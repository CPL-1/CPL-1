#ifndef __ELF32_H_INCLUDED__
#define __ELF32_H_INCLUDED__

#include <common/core/fd/fd.h>
#include <common/lib/dynarray.h>
#include <common/misc/types.h>

#define EI_NIDENT 16

struct Elf32_Header {
	unsigned char identifier[EI_NIDENT];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint32_t entry;
	uint32_t programHeadersFileOffset;
	uint32_t sectionHeadersFileOffset;
	uint32_t flags;
	uint16_t headerSize;
	uint16_t programHeaderSize;
	uint16_t programHeadersCount;
	uint16_t sectionHeaderSize;
	uint16_t sectionHeadersCount;
	uint16_t sectionNameTableIndex;
};

struct Elf32_ProgramHeader {
	uint32_t type;
	uint32_t offset;
	uint32_t virtualAddress;
	uint32_t physicalAddress;
	uint32_t fileSize;
	uint32_t memorySize;
	uint32_t flags;
	uint32_t align;
};

struct Elf32 {
	uint32_t entryPoint;
	struct Elf32_ProgramHeader *headers;
	size_t headersCount;
};

typedef bool (*Elf32_HeaderVerifyCallback)(struct Elf32_Header *);

struct Elf32 *Elf32_Parse(struct File *file, Elf32_HeaderVerifyCallback callback);
bool Elf32_LoadProgramHeaders(struct File *file, struct Elf32 *info);
void Elf32_Dispose(struct Elf32 *info);

#endif
