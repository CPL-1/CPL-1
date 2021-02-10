#include <common/core/devices/storage/mbr.h>
#include <common/core/devices/storage/partdev.h>
#include <common/core/devices/storage/storage.h>
#include <common/core/fd/fs/devfs.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>

// should be enough for everybody
#define GPT_MAX_PARTITIONS_COUNT 256

bool GPT_CheckDisk(struct Storage_Device *dev) {
	char magic[9];
	magic[8] = '\0';
	if (!Storage_ReadWrite(dev, 512, 8, magic, false)) {
		return false;
	}
	return StringsEqual(magic, "EFI PART");
}

struct GPT_Header {
	uint64_t signature;
	uint32_t revision;
	uint32_t headerSize;
	uint32_t CRC;
	uint32_t : 32;
	uint64_t headerLBA;
	uint64_t alternativeHeaderLBA;
	uint64_t firstUsableBlock;
	uint64_t lastUsableBlock;
	char GUID[16];
	uint64_t partitionTableLBA;
	uint32_t partitionEntriesCount;
	uint32_t partitionTableEntrySize;
	uint32_t partitionTableCRC;
} PACKED LITTLE_ENDIAN NOALIGN;

struct GPT_PartitionEntry {
	char typeGUID[16];
	char uniqueGUID[16];
	uint64_t startingLBA;
	uint64_t endingLBA;
	uint64_t attributes;
	char partitionName[72];
} PACKED LITTLE_ENDIAN NOALIGN;

bool GPT_IsEntryInUse(struct GPT_PartitionEntry *entry) {
	for (size_t i = 0; i < 16; ++i) {
		if (entry->typeGUID[i] != '\0') {
			return true;
		}
	}
	return false;
}

bool GPT_EnumeratePartitions(struct Storage_Device *dev) {
	struct GPT_Header header;
	if (!Storage_ReadWrite(dev, 512, sizeof(struct GPT_Header), (char *)&header, false)) {
		return false;
	}
	if (header.partitionTableEntrySize != sizeof(struct GPT_PartitionEntry)) {
		return false;
	}
	KernelLog_InfoMsg("GPT Partition Table Parser", "Number of detected partitions: %u",
					  (uint32_t)(header.partitionEntriesCount));
	if (header.partitionEntriesCount > GPT_MAX_PARTITIONS_COUNT) {
		KernelLog_WarnMsg("GPT Partition Table Parser",
						  "Parser detected that GPT has more than 256 partitions entries. Unfortunately, GPT parser "
						  "does not support that many partitions =(");
		return false;
	}
	struct VFS_Inode *partdevs[GPT_MAX_PARTITIONS_COUNT];
	struct GPT_PartitionEntry entries[GPT_MAX_PARTITIONS_COUNT];
	if (!Storage_ReadWrite(dev, header.partitionTableLBA * dev->sectorSize,
						   sizeof(struct GPT_PartitionEntry) * header.partitionEntriesCount, (char *)entries, false)) {
		return false;
	}
	for (size_t i = 0; i < header.partitionEntriesCount; ++i) {
		partdevs[i] = NULL;
		if (!GPT_IsEntryInUse(entries + i)) {
			continue;
		}
		if (entries[i].endingLBA < entries[i].startingLBA) {
			KernelLog_WarnMsg("GPT Partition Table Parser", "Ending LBA is somehow greater than starting LBA");
			continue;
		}
		entries[i].partitionName[71] = '\0';
		KernelLog_InfoMsg("GPT Partition Table Parser", "Partition: \"%s\" detected", entries[i].partitionName);
		partdevs[i] = PartDev_MakePartitionDevice(dev, entries[i].startingLBA * dev->sectorSize,
												  (entries[i].endingLBA - entries[i].startingLBA) * dev->sectorSize);
		if (partdevs[i] == NULL) {
			KernelLog_WarnMsg("GPT Partition Table Parser", "Failed to make partition device");
			for (size_t j = 0; j < i; ++j) {
				if (partdevs[j] != NULL) {
					FREE_OBJ(partdevs[j]);
				}
				return false;
			}
		}
	}
	for (size_t i = 0; i < 4; ++i) {
		if (partdevs[i] == NULL) {
			continue;
		}
		char buf[256];
		memset(buf, 0, 256);
		Storage_MakePartitionName(dev, buf, i);
		if (!DevFS_RegisterInode(buf, partdevs[i])) {
			FREE_OBJ(partdevs[i]);
		}
	}
	return true;
}
