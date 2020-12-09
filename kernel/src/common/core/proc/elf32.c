#include <common/core/fd/fd.h>
#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/elf32.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>
#include <hal/memory/virt.h>

#define PT_NULL 0
#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4
#define PF_MASK (PF_X | PF_W | PF_R)
#define EI_CLASS 4
#define EI_CLASS_32_BIT 1
#define ET_EXEC 0x02

struct Elf32 *Elf32_Parse(struct File *file, MAYBE_UNUSED Elf32_HeaderVerifyCallback callback) {
	struct Elf32_Header header;
	int result = File_Read(file, sizeof(struct Elf32_Header), (char *)&header);
	if (result != sizeof(struct Elf32_Header)) {
		return NULL;
	}
	if (header.identifier[0] != 0x7f || header.identifier[1] != (uint8_t)'E' || header.identifier[2] != (uint8_t)'L' ||
		header.identifier[3] != (uint8_t)'F') {
		return NULL;
	}
	if (header.identifier[EI_CLASS] != EI_CLASS_32_BIT) {
		return NULL;
	}
	if (header.type != ET_EXEC) {
		return NULL;
	}
	if (header.headerSize != sizeof(struct Elf32_Header)) {
		return NULL;
	}
	if (header.programHeaderSize != sizeof(struct Elf32_ProgramHeader)) {
		return NULL;
	}
	if (!callback(&header)) {
		return NULL;
	}
	int headersSize = header.programHeadersCount * sizeof(struct Elf32_ProgramHeader);
	struct Elf32_ProgramHeader *headers = Heap_AllocateMemory(headersSize);
	if (headers == NULL) {
		return NULL;
	}
	result = File_PRead(file, header.programHeadersFileOffset, headersSize, (char *)headers);
	if (result != headersSize) {
		Heap_FreeMemory(headers, headersSize);
		return NULL;
	}
	struct Elf32 *elf = ALLOC_OBJ(struct Elf32);
	if (elf == NULL) {
		Heap_FreeMemory(headers, headersSize);
		return NULL;
	}
	uintptr_t lastVaddrEnd = 0;
	for (uint16_t i = 0; i < header.programHeadersCount; ++i) {
		if (headers[i].type == PT_NULL) {
			continue;
		}
		if (headers[i].type != PT_LOAD) {
			FREE_OBJ(elf);
			Heap_FreeMemory(headers, headersSize);
			return NULL;
		}
		uintptr_t pageStart = ALIGN_DOWN(headers[i].virtualAddress, HAL_VirtualMM_PageSize);
		uintptr_t pageEnd = ALIGN_UP(headers[i].virtualAddress + headers[i].memorySize, HAL_VirtualMM_PageSize);
		if (pageEnd > (uint32_t)HAL_VirtualMM_UserAreaEnd || pageStart < (uint32_t)HAL_VirtualMM_UserAreaStart ||
			pageEnd < pageStart) {
			FREE_OBJ(elf);
			Heap_FreeMemory(headers, headersSize);
			return NULL;
		}
		if (pageStart < lastVaddrEnd) {
			FREE_OBJ(elf);
			Heap_FreeMemory(headers, headersSize);
			return NULL;
		}
		if ((headers[i].flags & ~PF_MASK) != 0) {
			FREE_OBJ(elf);
			Heap_FreeMemory(headers, headersSize);
			return NULL;
		}
		if (headers[i].fileSize > INT_MAX) {
			FREE_OBJ(elf);
			Heap_FreeMemory(headers, headersSize);
			return NULL;
		}
		lastVaddrEnd = pageEnd;
	}
	elf->entryPoint = header.entry;
	elf->headers = headers;
	elf->headersCount = header.programHeadersCount;
	return elf;
}

bool Elf32_LoadProgramHeaders(struct File *file, struct Elf32 *info) {
	for (size_t i = 0; i < info->headersCount; ++i) {
		uintptr_t pageStart = ALIGN_DOWN(info->headers[i].virtualAddress, HAL_VirtualMM_PageSize);
		uintptr_t pageEnd =
			ALIGN_UP(info->headers[i].virtualAddress + info->headers[i].memorySize, HAL_VirtualMM_PageSize);
		struct VirtualMM_MemoryRegionNode *region =
			VirtualMM_MemoryMap(NULL, pageStart, pageEnd - pageStart, HAL_VIRT_FLAGS_WRITABLE, true);
		if (region == NULL) {
			return false;
		}
		memset((void *)pageStart, 0, pageEnd - pageStart);
		int flags = 0;
		if ((info->headers[i].flags & PF_R) != 0) {
			flags |= HAL_VIRT_FLAGS_READABLE;
		}
		if ((info->headers[i].flags & PF_W) != 0) {
			flags |= HAL_VIRT_FLAGS_WRITABLE;
		}
		if ((info->headers[i].flags & PF_X) != 0) {
			flags |= HAL_VIRT_FLAGS_EXECUTABLE;
		}
		flags |= HAL_VIRT_FLAGS_USER_ACCESSIBLE;
		if (File_PReadUser(file, info->headers[i].offset, info->headers[i].fileSize,
						   (char *)(info->headers[i].virtualAddress)) != (int)(info->headers[i].fileSize)) {
			goto failure;
		}
		VirtualMM_MemoryRetype(NULL, region, flags);
		continue;
	failure:
		for (size_t j = 0; j < i; ++j) {
			uintptr_t start = info->headers[j].virtualAddress;
			uintptr_t end = ALIGN_UP(start + info->headers[j].memorySize, HAL_VirtualMM_PageSize);
			VirtualMM_MemoryUnmap(NULL, start, end - start, true);
		}
		return false;
	}
	return true;
}

void Elf32_Dispose(struct Elf32 *info) {
	Heap_FreeMemory(info->headers, info->headersCount * sizeof(struct Elf32_ProgramHeader));
	FREE_OBJ(info);
}