#include <common/core/fd/fs/devfs.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>
#include <common/core/storage/storage.h>
#include <common/drivers/storage/nvme.h>
#include <common/lib/kmsg.h>
#include <common/lib/math.h>
#include <common/misc/utils.h>
#include <hal/drivers/storage/nvme.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlock.h>

#define NVME_SUBMISSION_QUEUE_SIZE 2
#define NVME_COMPLETITION_QUEUE_SIZE 2
#define NVME_MAX_MDTS 8
#define NVME_QUEUE_PHYS_CONTIG (1 << 0) | (1 << 1)

static UINT32 NVME_ControllersDetected = 0;

struct NVME_CAPRegister {
	UINT32 mqes : 16;
	UINT32 cqr : 1;
	UINT32 amsIsWeightedRRSupported : 1;
	UINT32 amsVendorSpecific : 1;
	UINT32 : 5;
	UINT32 to : 8;
	UINT32 dstrd : 4;
	UINT32 nssrs : 1;
	UINT32 cssIsNvmSupported : 1;
	UINT32 : 6;
	UINT32 cssIsOnlyAdminSupported : 1;
	UINT32 bps : 1;
	UINT32 : 2;
	UINT32 mpsmin : 4;
	UINT32 mpsmax : 4;
	UINT32 pmrs : 1;
	UINT32 cmbs : 1;
	UINT32 : 6;
} PACKED_ALIGN(8) LITTLE_ENDIAN;

struct NVME_CC_Register {
	UINT32 en : 1;
	UINT32 : 3;
	UINT32 css : 3;
	UINT32 mps : 4;
	UINT32 ams : 3;
	UINT32 shn : 2;
	UINT32 iosqes : 4;
	UINT32 iocqes : 4;
	UINT32 : 8;
} PACKED_ALIGN(4) LITTLE_ENDIAN;

struct NVME_CSTSRegister {
	UINT32 rdy : 1;
	UINT32 cfs : 1;
	UINT32 shst : 2;
	UINT32 nssro : 1;
	UINT32 pp : 1;
	UINT32 : 26;
} PACKED_ALIGN(4) LITTLE_ENDIAN;

struct NVME_AQARegister {
	UINT32 asqs : 12;
	UINT32 : 4;
	UINT32 acqs : 12;
	UINT32 : 4;
} PACKED_ALIGN(4) LITTLE_ENDIAN;

struct NVME_ASQRegister {
	UINT64 : 12;
	UINT64 page : 52;
} PACKED_ALIGN(8) LITTLE_ENDIAN;

struct NVME_ACQRegister {
	UINT64 : 12;
	UINT64 page : 52;
} PACKED_ALIGN(8) LITTLE_ENDIAN;

enum NVME_AdminOpcode
{
	NVME_CREATE_SUBMISSION_QUEUE = 0x01,
	NVME_CREATE_COMPLETITION_QUEUE = 0x05,
	NVME_IDENTIFY = 0x06,
	NVME_SET_FEATURES = 0x09,
	NVME_GET_FEATURES = 0x0a
};

enum NVME_IOOpcode
{
	NVME_CMD_WRITE = 0x01,
	NVME_CMD_READ = 0x02
};

union NVME_SQEntry {
	struct {
		UINT8 opcode;
		UINT8 flags;
		UINT16 commandID;
		UINT32 nsid;
		UINT64 : 64;
		UINT64 : 64;
		UINT64 prp1;
		UINT64 prp2;
		UINT32 cns;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
	} PACKED LITTLE_ENDIAN identify;
	struct {
		UINT8 opcode;
		UINT8 flags;
		UINT16 commandID;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT64 prp1;
		UINT64 : 64;
		UINT16 sqid;
		UINT16 qsize;
		UINT16 sqFlags;
		UINT16 cqid;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
	} PACKED LITTLE_ENDIAN createSQ;
	struct {
		UINT8 opcode;
		UINT8 flags;
		UINT16 commandID;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT64 prp1;
		UINT64 : 64;
		UINT16 cqid;
		UINT16 qsize;
		UINT16 cqFlags;
		UINT16 irqVector;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
		UINT32 : 32;
	} PACKED LITTLE_ENDIAN createCQ;
	struct {
		UINT8 opcode;
		UINT8 flags;
		UINT16 commandID;
		UINT32 nsid;
		UINT64 rsvd2;
		UINT64 metadata;
		UINT64 prp1;
		UINT64 prp2;
		UINT64 slba;
		UINT16 length;
		UINT16 control;
		UINT32 dsmgmt;
		UINT32 reftag;
		UINT16 apptag;
		UINT16 appmask;
	} PACKED LITTLE_ENDIAN rw;
} PACKED LITTLE_ENDIAN;

struct NVME_CQEntry {
	UINT32 commandSpecific;
	UINT32 reserved;
	UINT16 submissionQueueHeadPointer;
	UINT16 submissionQueueIdentifier;
	UINT16 commandIdentifier;
	UINT16 phaseBit : 1;
	UINT16 status : 15;
} PACKED LITTLE_ENDIAN;

struct NVME_IDPowerState {
	UINT16 max_power;
	UINT8 rsvd2;
	UINT8 flags;
	UINT32 entryLat;
	UINT32 exitLat;
	UINT8 readTput;
	UINT8 readLat;
	UINT8 writeTput;
	UINT8 writeLat;
	UINT16 idlePower;
	UINT8 idleScale;
	UINT8 rsvd19;
	UINT16 activePower;
	UINT8 activeWorkScale;
	UINT8 rsvd23[9];
} PACKED LITTLE_ENDIAN;

struct NVME_IdentifyInfo {
	UINT16 vid;
	UINT16 ssvid;
	char sn[20];
	char mn[40];
	char fr[8];
	UINT8 rab;
	UINT8 ieee[3];
	UINT8 mic;
	UINT8 mdts;
	UINT16 cntlid;
	UINT32 ver;
	UINT8 rsvd84[172];
	UINT16 oacs;
	UINT8 acl;
	UINT8 aerl;
	UINT8 frmw;
	UINT8 lpa;
	UINT8 elpe;
	UINT8 npss;
	UINT8 avscc;
	UINT8 apsta;
	UINT16 wctemp;
	UINT16 cctemp;
	UINT8 rsvd270[242];
	UINT8 sqes;
	UINT8 cqes;
	UINT8 rsvd514[2];
	UINT32 nn;
	UINT16 oncs;
	UINT16 fuses;
	UINT8 fna;
	UINT8 vwc;
	UINT16 awun;
	UINT16 awupf;
	UINT8 nvscc;
	UINT8 rsvd531;
	UINT16 acwu;
	UINT8 rsvd534[2];
	UINT32 sgls;
	UINT8 rsvd540[1508];
	struct NVME_IDPowerState psd[32];
	UINT8 vs[1024];
} PACKED LITTLE_ENDIAN;

struct NVME_LBAF {
	UINT16 ms;
	UINT8 ds;
	UINT8 rp;
} LITTLE_ENDIAN;

struct NVME_NamespaceInfo {
	UINT64 nsze;
	UINT64 ncap;
	UINT64 nuse;
	UINT8 nsfeat;
	UINT8 nlbaf;
	UINT8 flbas;
	UINT8 mc;
	UINT8 dpc;
	UINT8 dps;
	UINT8 nmic;
	UINT8 rescap;
	UINT8 fpi;
	UINT8 rsvd33;
	UINT16 nawun;
	UINT16 nawupf;
	UINT16 nacwu;
	UINT16 nabsn;
	UINT16 nabo;
	UINT16 nabspf;
	UINT16 rsvd46;
	UINT64 nvmcap[2];
	UINT8 rsvd64[40];
	UINT8 nguid[16];
	UINT8 eui64[8];
	struct NVME_LBAF lbaf[16];
	UINT8 rsvd192[192];
	UINT8 vs[3712];
} PACKED LITTLE_ENDIAN;

struct NVMEDrive {
	struct hal_nvme_controller *controller;
	volatile UINT32 *bar0;
	union NVME_SQEntry *adminSubmissionQueue;
	volatile struct NVME_CQEntry *adminCompletitionQueue;
	union NVME_SQEntry *submissionQueue;
	volatile struct NVME_CQEntry *completitionQueue;
	volatile UINT16 *adminSubmissionDoorbell;
	volatile UINT16 *adminCompletitionDoorbell;
	volatile UINT16 *submissionDoorbell;
	volatile UINT16 *completitionDoorbell;
	struct NVME_IdentifyInfo *identifyInfo;
	USIZE adminSubmissionQueueHead;
	USIZE adminCompletitionQueueHead;
	USIZE submissionQueueHead;
	USIZE completitionQueueHead;
	struct nvme_drive_namespace *namespaces;
	UINT64 *prps;
	struct Mutex mutex;
	UINT32 id;
	bool adminPhaseBit;
	bool phaseBit;
	bool fallbackToPolling;
};

struct nvme_drive_namespace {
	struct Storage_Device cache;
	struct NVMEDrive *drive;
	struct NVME_NamespaceInfo *info;
	USIZE namespaceID;
	UINT64 block_size;
	UINT64 transferBlocksMax;
	UINT64 blocks_count;
};

static struct NVME_CAPRegister NVME_ReadCapRegister(volatile UINT32 *bar0) {
	struct NVME_CAPRegister result = {0};
	UINT64 *asPointer = (UINT64 *)&result;
	*asPointer = ((UINT64)bar0[1] << 32ULL) | (UINT64)bar0[0];
	return result;
}

static struct NVME_CC_Register NVME_ReadCCRegister(volatile UINT32 *bar0) {
	struct NVME_CC_Register result = {0};
	UINT32 *asPointer = (UINT32 *)&result;
	*asPointer = bar0[5];
	return result;
}

static struct NVME_CSTSRegister NVME_ReadCSTSRegister(volatile UINT32 *bar0) {
	struct NVME_CSTSRegister result = {0};
	UINT32 *asPointer = (UINT32 *)&result;
	*asPointer = bar0[7];
	return result;
}

static struct NVME_AQARegister NVME_ReadAQARegister(volatile UINT32 *bar0) {
	struct NVME_AQARegister result = {0};
	UINT32 *asPointer = (UINT32 *)&result;
	*asPointer = bar0[9];
	return result;
}

static struct NVME_ASQRegister NVME_ReadASQRegister(volatile UINT32 *bar0) {
	struct NVME_ASQRegister result = {0};
	UINT64 *asPointer = (UINT64 *)&result;
	*asPointer = ((UINT64)bar0[11] << 32ULL) | (UINT64)bar0[10];
	return result;
}

static struct NVME_ACQRegister NVME_ReadACQRegister(volatile UINT32 *bar0) {
	struct NVME_ACQRegister result = {0};
	UINT64 *asPointer = (UINT64 *)&result;
	*asPointer = ((UINT64)bar0[13] << 32ULL) | (UINT64)bar0[12];
	return result;
}

static void NVME_WriteCCRegister(volatile UINT32 *bar0, struct NVME_CC_Register cc) {
	UINT32 *asPointer = (UINT32 *)&cc;
	bar0[5] = *asPointer;
}

static void NVME_WriteAQARegister(volatile UINT32 *bar0, struct NVME_AQARegister aqa) {
	UINT32 *asPointer = (UINT32 *)&aqa;
	bar0[9] = *asPointer;
}

static void NVME_WriteASQRegister(volatile UINT32 *bar0, struct NVME_ASQRegister cap) {
	UINT64 *asPointer = (UINT64 *)&cap;
	bar0[10] = (UINT32)(*asPointer & 0xFFFFFFFF);
	bar0[11] = (UINT32)((*asPointer >> 32ULL) & 0xFFFFFFFF);
}

static void NVME_WriteACQRegister(volatile UINT32 *bar0, struct NVME_ACQRegister cap) {
	UINT64 *asPointer = (UINT64 *)&cap;
	bar0[12] = (UINT32)(*asPointer & 0xFFFFFFFF);
	bar0[13] = (UINT32)((*asPointer >> 32ULL) & 0xFFFFFFFF);
}

static void NVME_EnableInterrupts(volatile UINT32 *bar0) {
	bar0[0x03] = 0;
}

UNUSED static void NVME_DisableInterrupts(volatile UINT32 *bar0) {
	bar0[0x03] = 0xffffffff;
}

static UINT16 NVME_ExecuteAdminCmd(struct NVMEDrive *drive, union NVME_SQEntry command) {
	drive->adminSubmissionQueue[drive->adminSubmissionQueueHead] = command;
	drive->adminSubmissionQueueHead++;
	if (drive->adminSubmissionQueueHead == NVME_SUBMISSION_QUEUE_SIZE) {
		drive->adminSubmissionQueueHead = 0;
	}
	volatile struct NVME_CQEntry *completitionEntry = drive->adminCompletitionQueue + drive->adminCompletitionQueueHead;
	completitionEntry->phaseBit = drive->adminPhaseBit;

	*(drive->adminSubmissionDoorbell) = drive->adminSubmissionQueueHead;

	while (completitionEntry->phaseBit == drive->adminPhaseBit) {
		struct NVME_CSTSRegister csts;
		csts = NVME_ReadCSTSRegister(drive->bar0);
		if (csts.cfs == 1) {
			KernelLog_ErrorMsg("NVME Driver", "Fatal error occured while executing admin command");
		}
	}

	drive->adminCompletitionQueueHead++;
	if (drive->adminCompletitionQueueHead == NVME_COMPLETITION_QUEUE_SIZE) {
		drive->adminCompletitionQueueHead = 0;
		drive->adminPhaseBit = !(drive->adminPhaseBit);
	}

	*(drive->adminCompletitionDoorbell) = drive->adminCompletitionQueueHead;
	return drive->adminCompletitionQueue->status;
}

static void NVME_NotifyOnRecieve(struct NVMEDrive *drive) {
	drive->completitionQueueHead++;
	if (drive->completitionQueueHead == NVME_COMPLETITION_QUEUE_SIZE) {
		drive->completitionQueueHead = 0;
		drive->phaseBit = !(drive->phaseBit);
	}

	*(drive->completitionDoorbell) = drive->completitionQueueHead;
}

static UINT16 NVMEExecuteIOCommand(struct NVMEDrive *drive, union NVME_SQEntry command) {
	drive->submissionQueue[drive->submissionQueueHead] = command;
	drive->submissionQueueHead++;
	if (drive->submissionQueueHead == NVME_SUBMISSION_QUEUE_SIZE) {
		drive->submissionQueueHead = 0;
	}
	volatile struct NVME_CQEntry *completitionEntry = drive->completitionQueue + drive->completitionQueueHead;

	struct NVME_CSTSRegister csts;

	if (!drive->fallbackToPolling) {
		HAL_InterruptLock_Lock();
		completitionEntry->phaseBit = drive->phaseBit;
		*(drive->submissionDoorbell) = drive->submissionQueueHead;
		drive->controller->waitForEvent(drive->controller->ctx);
		csts = NVME_ReadCSTSRegister(drive->bar0);
		if (csts.cfs == 1) {
			KernelLog_ErrorMsg("NVME Driver", "Fatal error occured while executing IO command");
		}
	} else {
		completitionEntry->phaseBit = drive->phaseBit;
		*(drive->submissionDoorbell) = drive->submissionQueueHead;
		while (completitionEntry->phaseBit == drive->phaseBit) {
			struct NVME_CSTSRegister csts;
			csts = NVME_ReadCSTSRegister(drive->bar0);
			if (csts.cfs == 1) {
				KernelLog_ErrorMsg("NVME Driver", "Fatal error occured while executing IO command");
			}
		}
		NVME_NotifyOnRecieve(drive);
	}
	return drive->completitionQueue->status;
}

static bool nvme_rw_lba(struct NVMEDrive *drive, USIZE ns, void *buf, UINT64 lba, USIZE count, bool write) {
	Mutex_Lock(&(drive->mutex));
	struct nvme_drive_namespace *namespace = drive->namespaces + (ns - 1);
	USIZE block_size = namespace->block_size;
	UINTN page_offset = (UINTN)buf & (HAL_VirtualMM_PageSize - 1);

	UNUSED union NVME_SQEntry rw_command;
	rw_command.rw.opcode = write ? NVME_CMD_WRITE : NVME_CMD_READ;
	rw_command.rw.flags = 0;
	rw_command.rw.control = 0;
	rw_command.rw.dsmgmt = 0;
	rw_command.rw.reftag = 0;
	rw_command.rw.apptag = 0;
	rw_command.rw.appmask = 0;
	rw_command.rw.metadata = 0;
	rw_command.rw.slba = lba;
	rw_command.rw.nsid = ns;
	rw_command.rw.length = count - 1;
	UINTN alignedDown = ALIGN_DOWN((UINTN)buf, HAL_VirtualMM_PageSize);
	UINTN alignedUp = ALIGN_UP((UINTN)(buf + count * block_size), HAL_VirtualMM_PageSize);
	USIZE prpEntriesCount = ((alignedUp - alignedDown) / HAL_VirtualMM_PageSize) - 1;

	if (prpEntriesCount == 0) {
		rw_command.rw.prp1 = (UINT64)((UINTN)buf - HAL_VirtualMM_KernelMappingBase);
	} else if (prpEntriesCount == 1) {
		rw_command.rw.prp1 = (UINT64)((UINTN)buf - HAL_VirtualMM_KernelMappingBase);
		rw_command.rw.prp2 =
			(UINT64)((UINTN)buf - HAL_VirtualMM_KernelMappingBase - page_offset + HAL_VirtualMM_PageSize);
	} else {
		rw_command.rw.prp1 = (UINT64)((UINTN)buf - HAL_VirtualMM_KernelMappingBase);
		rw_command.rw.prp2 = (UINT64)((UINTN)(drive->prps) - HAL_VirtualMM_KernelMappingBase);
		for (USIZE i = 0; i < prpEntriesCount; ++i) {
			drive->prps[i] =
				(UINT64)((UINTN)buf - HAL_VirtualMM_KernelMappingBase - page_offset + (i + 1) * HAL_VirtualMM_PageSize);
		}
	}

	UINT16 status = NVMEExecuteIOCommand(drive, rw_command);
	Mutex_Unlock(&(drive->mutex));
	return status == 0;
}

static bool nvme_namespace_rw_lba(void *ctx, char *buf, UINT64 lba, USIZE count, bool write) {
	struct nvme_drive_namespace *namespace = (struct nvme_drive_namespace *)ctx;
	return nvme_rw_lba(namespace->drive, namespace->namespaceID, buf, lba, count, write);
}

bool NVME_Initialize(struct hal_nvme_controller *controller) {
	struct NVMEDrive *nvmeDriveInfo = ALLOC_OBJ(struct NVMEDrive);
	if (nvmeDriveInfo == NULL) {
		return false;
	}
	nvmeDriveInfo->adminCompletitionQueueHead = 0;
	nvmeDriveInfo->adminSubmissionQueueHead = 0;
	nvmeDriveInfo->completitionQueueHead = 0;
	nvmeDriveInfo->submissionQueueHead = 0;
	nvmeDriveInfo->controller = controller;
	nvmeDriveInfo->id = NVME_ControllersDetected;
	nvmeDriveInfo->fallbackToPolling = true;
	NVME_ControllersDetected++;
	Mutex_Initialize(&(nvmeDriveInfo->mutex));

	UINTN mappingPaddr = ALIGN_DOWN(controller->offset, HAL_VirtualMM_PageSize);
	USIZE mappingSize = ALIGN_UP(controller->size + (controller->offset - mappingPaddr), HAL_VirtualMM_PageSize);
	UINTN mapping = HAL_VirtualMM_GetIOMapping(mappingPaddr, mappingSize, controller->disableCache);
	if (mapping == 0) {
		KernelLog_WarnMsg("NVME Driver", "Failed to map BAR0");
		return false;
	}
	volatile UINT32 *bar0 = (volatile UINT32 *)(mapping + (controller->offset - mappingPaddr));
	nvmeDriveInfo->bar0 = bar0;

	struct NVME_CC_Register cc;
	struct NVME_CSTSRegister csts;
	struct NVME_CAPRegister cap;
	struct NVME_AQARegister aqa;
	struct NVME_ASQRegister asq;
	struct NVME_ACQRegister acq;

	cc = NVME_ReadCCRegister(bar0);
	cc.en = 0;
	NVME_WriteCCRegister(bar0, cc);
	csts = NVME_ReadCSTSRegister(bar0);
	while (csts.rdy != 0) {
		csts = NVME_ReadCSTSRegister(bar0);
	}
	KernelLog_InfoMsg("NVME Driver", "NVME Controller disabled");

	cap = NVME_ReadCapRegister(bar0);
	if (!cap.cssIsNvmSupported) {
		KernelLog_WarnMsg("NVME Driver", "NVME controller doesn't support NVM command set");
		return false;
	}
	if (cap.mpsmin > (MATH_LOG2_ROUNDUP(HAL_VirtualMM_PageSize) - 12)) {
		KernelLog_WarnMsg("NVME Driver", "NVME controller doesn't support host page size");
		return false;
	}
	if (cap.mpsmax < (MATH_LOG2_ROUNDUP(HAL_VirtualMM_PageSize) - 12)) {
		KernelLog_WarnMsg("NVME Driver", "NVME controller doesn't support host page size");
	}
	USIZE submissionQueueSize =
		ALIGN_UP(sizeof(union NVME_SQEntry) * NVME_SUBMISSION_QUEUE_SIZE, HAL_VirtualMM_PageSize);
	union NVME_SQEntry *adminSubmissionQueue = (union NVME_SQEntry *)Heap_AllocateMemory(submissionQueueSize);
	union NVME_SQEntry *submissionQueue = (union NVME_SQEntry *)Heap_AllocateMemory(submissionQueueSize);

	USIZE completitionQueueSize =
		ALIGN_UP(sizeof(struct NVME_CQEntry) * NVME_COMPLETITION_QUEUE_SIZE, HAL_VirtualMM_PageSize);
	struct NVME_CQEntry *adminCompletitionQueue = (struct NVME_CQEntry *)Heap_AllocateMemory(completitionQueueSize);
	struct NVME_CQEntry *completitionQueue = (struct NVME_CQEntry *)Heap_AllocateMemory(completitionQueueSize);

	if (adminSubmissionQueue == NULL || adminCompletitionQueue == NULL || submissionQueue == NULL ||
		completitionQueue == NULL) {
		KernelLog_WarnMsg("NVME Driver", "Failed to allocate queues for NVME controller");
		return false;
	}
	nvmeDriveInfo->adminSubmissionQueue = adminSubmissionQueue;
	nvmeDriveInfo->adminCompletitionQueue = adminCompletitionQueue;
	nvmeDriveInfo->submissionQueue = submissionQueue;
	nvmeDriveInfo->completitionQueue = completitionQueue;
	nvmeDriveInfo->adminPhaseBit = false;
	nvmeDriveInfo->phaseBit = false;
	memset(adminSubmissionQueue, 0, submissionQueueSize);
	memset(adminCompletitionQueue, 0, completitionQueueSize);
	memset(submissionQueue, 0, submissionQueueSize);
	memset(completitionQueue, 0, completitionQueueSize);
	KernelLog_InfoMsg("NVME Driver", "NVME controller admin queues allocated");

	UINTN adminSubmissionQueuePhysical = (UINTN)adminSubmissionQueue - HAL_VirtualMM_KernelMappingBase;
	UINTN adminCompletitionQueuePhysical = (UINTN)adminCompletitionQueue - HAL_VirtualMM_KernelMappingBase;
	UINTN submissionQueuePhysical = (UINTN)submissionQueue - HAL_VirtualMM_KernelMappingBase;
	UINTN completitionQueuePhysical = (UINTN)completitionQueue - HAL_VirtualMM_KernelMappingBase;

	asq = NVME_ReadASQRegister(bar0);
	acq = NVME_ReadACQRegister(bar0);
	asq.page = adminSubmissionQueuePhysical / HAL_VirtualMM_PageSize;
	acq.page = adminCompletitionQueuePhysical / HAL_VirtualMM_PageSize;
	NVME_WriteASQRegister(bar0, asq);
	NVME_WriteACQRegister(bar0, acq);

	aqa = NVME_ReadAQARegister(bar0);
	aqa.acqs = NVME_COMPLETITION_QUEUE_SIZE - 1;
	aqa.asqs = NVME_SUBMISSION_QUEUE_SIZE - 1;
	NVME_WriteAQARegister(bar0, aqa);

	cc = NVME_ReadCCRegister(bar0);
	cc.ams = 0;
	cc.mps = (MATH_LOG2_ROUNDUP(HAL_VirtualMM_PageSize) - 12);
	cc.css = 0;
	cc.shn = 0;
	cc.iocqes = 4;
	cc.iosqes = 6;
	NVME_WriteCCRegister(bar0, cc);

	cc = NVME_ReadCCRegister(bar0);
	cc.en = 1;
	NVME_WriteCCRegister(bar0, cc);

	csts = NVME_ReadCSTSRegister(bar0);
	while (csts.rdy != 1) {
		if (csts.cfs != 0) {
			KernelLog_ErrorMsg("NVME Driver", "Error while enabling controller");
		}
		csts = NVME_ReadCSTSRegister(bar0);
	}

	KernelLog_InfoMsg("NVME Driver", "NVME Controller reenabled");

	USIZE doorbellSize = 1 << (2 + cap.dstrd);
	UINTN doorbellsBase = (UINT32)bar0 + 0x1000;
	nvmeDriveInfo->adminSubmissionDoorbell = (volatile UINT16 *)(doorbellsBase + doorbellSize * 0);
	nvmeDriveInfo->adminCompletitionDoorbell = (volatile UINT16 *)(doorbellsBase + doorbellSize * 1);
	nvmeDriveInfo->submissionDoorbell = (volatile UINT16 *)(doorbellsBase + doorbellSize * 2);
	nvmeDriveInfo->completitionDoorbell = (volatile UINT16 *)(doorbellsBase + doorbellSize * 3);

	union NVME_SQEntry identifyCommand = {0};
	identifyCommand.identify.opcode = NVME_IDENTIFY;
	identifyCommand.identify.nsid = 0;
	identifyCommand.identify.cns = 1;
	struct NVME_IdentifyInfo *identifyInfo = ALLOC_OBJ(struct NVME_IdentifyInfo);
	if (identifyInfo == NULL) {
		KernelLog_WarnMsg("NVME Driver", "Failed to allocate identify info object");
		return false;
	}
	identifyCommand.identify.prp1 = (UINTN)identifyInfo - HAL_VirtualMM_KernelMappingBase;
	identifyCommand.identify.prp2 = 0;
	if (NVME_ExecuteAdminCmd(nvmeDriveInfo, identifyCommand) != 0) {
		KernelLog_WarnMsg("NVME Driver", "Failed to execute identify command");
		return false;
	}
	nvmeDriveInfo->identifyInfo = (struct NVME_IdentifyInfo *)identifyInfo;
	KernelLog_InfoMsg("NVME Driver", "Identify command executed successfully");

	union NVME_SQEntry createQueueCommand = {0};

	createQueueCommand.createCQ.opcode = NVME_CREATE_COMPLETITION_QUEUE;
	createQueueCommand.createCQ.prp1 = completitionQueuePhysical;
	createQueueCommand.createCQ.cqid = 1;
	createQueueCommand.createCQ.qsize = NVME_COMPLETITION_QUEUE_SIZE - 1;
	createQueueCommand.createCQ.cqFlags = NVME_QUEUE_PHYS_CONTIG;
	createQueueCommand.createCQ.irqVector = 0;
	if (NVME_ExecuteAdminCmd(nvmeDriveInfo, createQueueCommand) != 0) {
		KernelLog_WarnMsg("NVME Driver", "Failed to create completition queue");
		return false;
	}
	KernelLog_InfoMsg("NVME Driver", "Created completition queue");

	createQueueCommand.createSQ.opcode = NVME_CREATE_SUBMISSION_QUEUE;
	createQueueCommand.createSQ.prp1 = submissionQueuePhysical;
	createQueueCommand.createSQ.cqid = 1;
	createQueueCommand.createSQ.sqid = 1;
	createQueueCommand.createSQ.qsize = NVME_SUBMISSION_QUEUE_SIZE - 1;
	createQueueCommand.createSQ.sqFlags = NVME_QUEUE_PHYS_CONTIG;
	if (NVME_ExecuteAdminCmd(nvmeDriveInfo, createQueueCommand) != 0) {
		KernelLog_WarnMsg("NVME Driver", "Failed to create submission queue");
		return false;
	}
	KernelLog_InfoMsg("NVME Driver", "Created submission queue");

	USIZE namespacesCount = identifyInfo->nn;
	KernelLog_InfoMsg("NVME Driver", "NVME Drive namespaces count: %u", identifyInfo->nn);
	struct nvme_drive_namespace *namespaces =
		(struct nvme_drive_namespace *)Heap_AllocateMemory(sizeof(struct nvme_drive_namespace) * namespacesCount);
	if (namespaces == NULL) {
		KernelLog_WarnMsg("NVME Driver", "Failed to allocate namespaces objects");
		return false;
	}
	nvmeDriveInfo->namespaces = namespaces;

	USIZE maxSize = 1 << (12 + nvmeDriveInfo->identifyInfo->mdts);
	if (nvmeDriveInfo->identifyInfo->mdts > NVME_MAX_MDTS || maxSize == 4096) {
		maxSize = 1 << (12 + NVME_MAX_MDTS);
	}
	KernelLog_InfoMsg("NVME Driver", "Max transfer size: %u", maxSize);

	UINT64 *prps = Heap_AllocateMemory((maxSize / HAL_VirtualMM_PageSize) * 8);
	if (prps == NULL) {
		KernelLog_WarnMsg("NVME Driver", "Failed to allocate PRP list");
		return false;
	}

	nvmeDriveInfo->prps = prps;

	for (USIZE i = 0; i < namespacesCount; ++i) {
		struct nvme_drive_namespace *namespace = namespaces + i;
		namespace->namespaceID = i + 1;
		namespace->drive = nvmeDriveInfo;
		namespace->info = ALLOC_OBJ(struct NVME_NamespaceInfo);
		if (namespace->info == NULL) {
			KernelLog_WarnMsg("NVME Driver", "Failed to allocate namespace info object");
			return false;
		}

		union NVME_SQEntry getNamespaceInfoCommand = {0};
		getNamespaceInfoCommand.identify.opcode = NVME_IDENTIFY;
		getNamespaceInfoCommand.identify.nsid = i + 1;
		getNamespaceInfoCommand.identify.cns = 0;
		getNamespaceInfoCommand.identify.prp1 = (UINTN) namespace->info - HAL_VirtualMM_KernelMappingBase;
		if (NVME_ExecuteAdminCmd(nvmeDriveInfo, getNamespaceInfoCommand) != 0) {
			KernelLog_WarnMsg("NVME Driver", "Failed to read namespace info");
			return false;
		}
		KernelLog_InfoMsg("NVME Driver", "Successfully read NVME namespace info");

		UINT8 formatUsed = namespace->info->flbas & 0b11;
		USIZE blockSize = 1 << namespace->info->lbaf[formatUsed].ds;
		USIZE transferBlocksMax = maxSize / blockSize;

		namespace->block_size = blockSize;
		namespace->transferBlocksMax = transferBlocksMax;
		namespace->blocks_count = namespace->info->nsze;

		namespace->cache.sectorSize = namespace->block_size;
		namespace->cache.maxRWSectorsCount = namespace->transferBlocksMax;
		namespace->cache.sectorsCount = namespace->blocks_count;
		namespace->cache.rw_lba = nvme_namespace_rw_lba;
		namespace->cache.ctx = (void *)namespace;
		memset(namespace->cache.name, 0, 256);
		sprintf("nvme%un%u\0", namespace->cache.name, 256, nvmeDriveInfo->id, i + 1);
		namespace->cache.partitioningScheme = STORAGE_P_NUMERIC_PART_NAMING;
		if (!storageInit(&(namespace->cache))) {
			KernelLog_WarnMsg("NVME Driver", "Failed to add drive object to storage stack");
			return false;
		}

		KernelLog_InfoMsg("NVME Driver", "Drive namespace %u initialized!", i + 1);
	}

	nvmeDriveInfo->fallbackToPolling =
		!controller->initEvent(controller->ctx, (void (*)(void *))NVME_NotifyOnRecieve, (void *)nvmeDriveInfo);
	if (!(nvmeDriveInfo->fallbackToPolling)) {
		NVME_EnableInterrupts(bar0);
	}

	return true;
}