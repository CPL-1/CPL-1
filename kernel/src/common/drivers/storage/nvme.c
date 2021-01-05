#include <common/core/fd/fs/devfs.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>
#include <common/core/devices/storage/storage.h>
#include <common/drivers/storage/nvme.h>
#include <common/lib/kmsg.h>
#include <common/lib/math.h>
#include <common/misc/utils.h>
#include <hal/drivers/storage/nvme.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlevel.h>

#define NVME_SUBMISSION_QUEUE_SIZE 2
#define NVME_COMPLETITION_QUEUE_SIZE 2
#define NVME_MAX_MDTS 8
#define NVME_QUEUE_PHYS_CONTIG (1 << 0) | (1 << 1)

static uint32_t m_controllersDetected = 0;

struct NVME_CAPRegister {
	uint32_t mqes : 16;
	uint32_t cqr : 1;
	uint32_t amsIsWeightedRRSupported : 1;
	uint32_t amsVendorSpecific : 1;
	uint32_t : 5;
	uint32_t to : 8;
	uint32_t dstrd : 4;
	uint32_t nssrs : 1;
	uint32_t cssIsNvmSupported : 1;
	uint32_t : 6;
	uint32_t cssIsOnlyAdminSupported : 1;
	uint32_t bps : 1;
	uint32_t : 2;
	uint32_t mpsmin : 4;
	uint32_t mpsmax : 4;
	uint32_t pmrs : 1;
	uint32_t cmbs : 1;
	uint32_t : 6;
} PACKED_ALIGN(8) LITTLE_ENDIAN;

struct NVME_CC_Register {
	uint32_t en : 1;
	uint32_t : 3;
	uint32_t css : 3;
	uint32_t mps : 4;
	uint32_t ams : 3;
	uint32_t shn : 2;
	uint32_t iosqes : 4;
	uint32_t iocqes : 4;
	uint32_t : 8;
} PACKED_ALIGN(4) LITTLE_ENDIAN;

struct NVME_CSTSRegister {
	uint32_t rdy : 1;
	uint32_t cfs : 1;
	uint32_t shst : 2;
	uint32_t nssro : 1;
	uint32_t pp : 1;
	uint32_t : 26;
} PACKED_ALIGN(4) LITTLE_ENDIAN;

struct NVME_AQARegister {
	uint32_t asqs : 12;
	uint32_t : 4;
	uint32_t acqs : 12;
	uint32_t : 4;
} PACKED_ALIGN(4) LITTLE_ENDIAN;

struct NVME_ASQRegister {
	uint64_t : 12;
	uint64_t page : 52;
} PACKED_ALIGN(8) LITTLE_ENDIAN;

struct NVME_ACQRegister {
	uint64_t : 12;
	uint64_t page : 52;
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
		uint8_t opcode;
		uint8_t flags;
		uint16_t commandID;
		uint32_t nsid;
		uint64_t : 64;
		uint64_t : 64;
		uint64_t prp1;
		uint64_t prp2;
		uint32_t cns;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
	} PACKED LITTLE_ENDIAN identify;
	struct {
		uint8_t opcode;
		uint8_t flags;
		uint16_t commandID;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint64_t prp1;
		uint64_t : 64;
		uint16_t sqid;
		uint16_t qsize;
		uint16_t sqFlags;
		uint16_t cqid;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
	} PACKED LITTLE_ENDIAN createSQ;
	struct {
		uint8_t opcode;
		uint8_t flags;
		uint16_t commandID;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint64_t prp1;
		uint64_t : 64;
		uint16_t cqid;
		uint16_t qsize;
		uint16_t cqFlags;
		uint16_t irqVector;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
	} PACKED LITTLE_ENDIAN createCQ;
	struct {
		uint8_t opcode;
		uint8_t flags;
		uint16_t commandID;
		uint32_t nsid;
		uint64_t rsvd2;
		uint64_t metadata;
		uint64_t prp1;
		uint64_t prp2;
		uint64_t slba;
		uint16_t length;
		uint16_t control;
		uint32_t dsmgmt;
		uint32_t reftag;
		uint16_t apptag;
		uint16_t appmask;
	} PACKED LITTLE_ENDIAN rw;
} PACKED LITTLE_ENDIAN;

struct NVME_CQEntry {
	uint32_t commandSpecific;
	uint32_t reserved;
	uint16_t submissionQueueHeadPointer;
	uint16_t submissionQueueIdentifier;
	uint16_t commandIdentifier;
	uint16_t phaseBit : 1;
	uint16_t status : 15;
} PACKED LITTLE_ENDIAN;

struct NVME_IDPowerState {
	uint16_t max_power;
	uint8_t rsvd2;
	uint8_t flags;
	uint32_t entryLat;
	uint32_t exitLat;
	uint8_t readTput;
	uint8_t readLat;
	uint8_t writeTput;
	uint8_t writeLat;
	uint16_t idlePower;
	uint8_t idleScale;
	uint8_t rsvd19;
	uint16_t activePower;
	uint8_t activeWorkScale;
	uint8_t rsvd23[9];
} PACKED LITTLE_ENDIAN;

struct NVME_IdentifyInfo {
	uint16_t vid;
	uint16_t ssvid;
	char sn[20];
	char mn[40];
	char fr[8];
	uint8_t rab;
	uint8_t ieee[3];
	uint8_t mic;
	uint8_t mdts;
	uint16_t cntlid;
	uint32_t ver;
	uint8_t rsvd84[172];
	uint16_t oacs;
	uint8_t acl;
	uint8_t aerl;
	uint8_t frmw;
	uint8_t lpa;
	uint8_t elpe;
	uint8_t npss;
	uint8_t avscc;
	uint8_t apsta;
	uint16_t wctemp;
	uint16_t cctemp;
	uint8_t rsvd270[242];
	uint8_t sqes;
	uint8_t cqes;
	uint8_t rsvd514[2];
	uint32_t nn;
	uint16_t oncs;
	uint16_t fuses;
	uint8_t fna;
	uint8_t vwc;
	uint16_t awun;
	uint16_t awupf;
	uint8_t nvscc;
	uint8_t rsvd531;
	uint16_t acwu;
	uint8_t rsvd534[2];
	uint32_t sgls;
	uint8_t rsvd540[1508];
	struct NVME_IDPowerState psd[32];
	uint8_t vs[1024];
} PACKED LITTLE_ENDIAN;

struct NVME_LBAF {
	uint16_t ms;
	uint8_t ds;
	uint8_t rp;
} LITTLE_ENDIAN;

struct NVME_NamespaceInfo {
	uint64_t nsze;
	uint64_t ncap;
	uint64_t nuse;
	uint8_t nsfeat;
	uint8_t nlbaf;
	uint8_t flbas;
	uint8_t mc;
	uint8_t dpc;
	uint8_t dps;
	uint8_t nmic;
	uint8_t rescap;
	uint8_t fpi;
	uint8_t rsvd33;
	uint16_t nawun;
	uint16_t nawupf;
	uint16_t nacwu;
	uint16_t nabsn;
	uint16_t nabo;
	uint16_t nabspf;
	uint16_t rsvd46;
	uint64_t nvmcap[2];
	uint8_t rsvd64[40];
	uint8_t nguid[16];
	uint8_t eui64[8];
	struct NVME_LBAF lbaf[16];
	uint8_t rsvd192[192];
	uint8_t vs[3712];
} PACKED LITTLE_ENDIAN;

struct NVMEDrive {
	struct HAL_NVMEController *controller;
	VOLATILE uint32_t *bar0;
	union NVME_SQEntry *adminSubmissionQueue;
	VOLATILE struct NVME_CQEntry *adminCompletitionQueue;
	union NVME_SQEntry *submissionQueue;
	VOLATILE struct NVME_CQEntry *completitionQueue;
	VOLATILE uint16_t *adminSubmissionDoorbell;
	VOLATILE uint16_t *adminCompletitionDoorbell;
	VOLATILE uint16_t *submissionDoorbell;
	VOLATILE uint16_t *completitionDoorbell;
	struct NVME_IdentifyInfo *identifyInfo;
	size_t adminSubmissionQueueHead;
	size_t adminCompletitionQueueHead;
	size_t submissionQueueHead;
	size_t completitionQueueHead;
	struct nvme_drive_namespace *namespaces;
	uint64_t *prps;
	struct Mutex mutex;
	uint32_t id;
	bool adminPhaseBit;
	bool phaseBit;
	bool fallbackToPolling;
};

struct nvme_drive_namespace {
	struct Storage_Device cache;
	struct NVMEDrive *drive;
	struct NVME_NamespaceInfo *info;
	size_t namespaceID;
	uint64_t block_size;
	uint64_t transferBlocksMax;
	uint64_t blocks_count;
};

static struct NVME_CAPRegister NVME_ReadCapRegister(VOLATILE uint32_t *bar0) {
	struct NVME_CAPRegister result = {0};
	uint64_t *asPointer = (uint64_t *)&result;
	*asPointer = ((uint64_t)bar0[1] << 32ULL) | (uint64_t)bar0[0];
	return result;
}

static struct NVME_CC_Register NVME_ReadCCRegister(VOLATILE uint32_t *bar0) {
	struct NVME_CC_Register result = {0};
	uint32_t *asPointer = (uint32_t *)&result;
	*asPointer = bar0[5];
	return result;
}

static struct NVME_CSTSRegister NVME_ReadCSTSRegister(VOLATILE uint32_t *bar0) {
	struct NVME_CSTSRegister result = {0};
	uint32_t *asPointer = (uint32_t *)&result;
	*asPointer = bar0[7];
	return result;
}

static struct NVME_AQARegister NVME_ReadAQARegister(VOLATILE uint32_t *bar0) {
	struct NVME_AQARegister result = {0};
	uint32_t *asPointer = (uint32_t *)&result;
	*asPointer = bar0[9];
	return result;
}

static struct NVME_ASQRegister NVME_ReadASQRegister(VOLATILE uint32_t *bar0) {
	struct NVME_ASQRegister result = {0};
	uint64_t *asPointer = (uint64_t *)&result;
	*asPointer = ((uint64_t)bar0[11] << 32ULL) | (uint64_t)bar0[10];
	return result;
}

static struct NVME_ACQRegister NVME_ReadACQRegister(VOLATILE uint32_t *bar0) {
	struct NVME_ACQRegister result = {0};
	uint64_t *asPointer = (uint64_t *)&result;
	*asPointer = ((uint64_t)bar0[13] << 32ULL) | (uint64_t)bar0[12];
	return result;
}

static void NVME_WriteCCRegister(VOLATILE uint32_t *bar0, struct NVME_CC_Register cc) {
	uint32_t *asPointer = (uint32_t *)&cc;
	bar0[5] = *asPointer;
}

static void NVME_WriteAQARegister(VOLATILE uint32_t *bar0, struct NVME_AQARegister aqa) {
	uint32_t *asPointer = (uint32_t *)&aqa;
	bar0[9] = *asPointer;
}

static void NVME_WriteASQRegister(VOLATILE uint32_t *bar0, struct NVME_ASQRegister cap) {
	uint64_t *asPointer = (uint64_t *)&cap;
	bar0[10] = (uint32_t)(*asPointer & 0xFFFFFFFF);
	bar0[11] = (uint32_t)((*asPointer >> 32ULL) & 0xFFFFFFFF);
}

static void NVME_WriteACQRegister(VOLATILE uint32_t *bar0, struct NVME_ACQRegister cap) {
	uint64_t *asPointer = (uint64_t *)&cap;
	bar0[12] = (uint32_t)(*asPointer & 0xFFFFFFFF);
	bar0[13] = (uint32_t)((*asPointer >> 32ULL) & 0xFFFFFFFF);
}

static void NVME_EnableInterrupts(VOLATILE uint32_t *bar0) {
	bar0[0x03] = 0;
}

MAYBE_UNUSED static void NVME_DisableInterrupts(VOLATILE uint32_t *bar0) {
	bar0[0x03] = 0xffffffff;
}

static uint16_t NVME_ExecuteAdminCmd(struct NVMEDrive *drive, union NVME_SQEntry command) {
	drive->adminSubmissionQueue[drive->adminSubmissionQueueHead] = command;
	drive->adminSubmissionQueueHead++;
	if (drive->adminSubmissionQueueHead == NVME_SUBMISSION_QUEUE_SIZE) {
		drive->adminSubmissionQueueHead = 0;
	}
	VOLATILE struct NVME_CQEntry *completitionEntry = drive->adminCompletitionQueue + drive->adminCompletitionQueueHead;
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

static uint16_t NVMEExecuteIOCommand(struct NVMEDrive *drive, union NVME_SQEntry command) {
	drive->submissionQueue[drive->submissionQueueHead] = command;
	drive->submissionQueueHead++;
	if (drive->submissionQueueHead == NVME_SUBMISSION_QUEUE_SIZE) {
		drive->submissionQueueHead = 0;
	}
	VOLATILE struct NVME_CQEntry *completitionEntry = drive->completitionQueue + drive->completitionQueueHead;

	struct NVME_CSTSRegister csts;

	if (!drive->fallbackToPolling) {
		HAL_InterruptLevel_Elevate();
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

static bool NVME_ReadWriteLBA(struct NVMEDrive *drive, size_t ns, void *buf, uint64_t lba, size_t count, bool write) {
	Mutex_Lock(&(drive->mutex));
	struct nvme_drive_namespace *namespace = drive->namespaces + (ns - 1);
	size_t block_size = namespace->block_size;
	uintptr_t page_offset = (uintptr_t)buf & (HAL_VirtualMM_PageSize - 1);

	union NVME_SQEntry rwCommand;
	rwCommand.rw.opcode = write ? NVME_CMD_WRITE : NVME_CMD_READ;
	rwCommand.rw.flags = 0;
	rwCommand.rw.control = 0;
	rwCommand.rw.dsmgmt = 0;
	rwCommand.rw.reftag = 0;
	rwCommand.rw.apptag = 0;
	rwCommand.rw.appmask = 0;
	rwCommand.rw.metadata = 0;
	rwCommand.rw.slba = lba;
	rwCommand.rw.nsid = ns;
	rwCommand.rw.length = count - 1;
	uintptr_t alignedDown = ALIGN_DOWN((uintptr_t)buf, HAL_VirtualMM_PageSize);
	uintptr_t alignedUp = ALIGN_UP((uintptr_t)(buf + count * block_size), HAL_VirtualMM_PageSize);
	size_t prpEntriesCount = ((alignedUp - alignedDown) / HAL_VirtualMM_PageSize) - 1;

	if (prpEntriesCount == 0) {
		rwCommand.rw.prp1 = (uint64_t)((uintptr_t)buf - HAL_VirtualMM_KernelMappingBase);
	} else if (prpEntriesCount == 1) {
		rwCommand.rw.prp1 = (uint64_t)((uintptr_t)buf - HAL_VirtualMM_KernelMappingBase);
		rwCommand.rw.prp2 =
			(uint64_t)((uintptr_t)buf - HAL_VirtualMM_KernelMappingBase - page_offset + HAL_VirtualMM_PageSize);
	} else {
		rwCommand.rw.prp1 = (uint64_t)((uintptr_t)buf - HAL_VirtualMM_KernelMappingBase);
		rwCommand.rw.prp2 = (uint64_t)((uintptr_t)(drive->prps) - HAL_VirtualMM_KernelMappingBase);
		for (size_t i = 0; i < prpEntriesCount; ++i) {
			drive->prps[i] = (uint64_t)((uintptr_t)buf - HAL_VirtualMM_KernelMappingBase - page_offset +
										(i + 1) * HAL_VirtualMM_PageSize);
		}
	}

	uint16_t status = NVMEExecuteIOCommand(drive, rwCommand);
	Mutex_Unlock(&(drive->mutex));
	return status == 0;
}

static bool NVME_NamespaceReadWriteLBA(void *ctx, char *buf, uint64_t lba, size_t count, bool write) {
	struct nvme_drive_namespace *namespace = (struct nvme_drive_namespace *)ctx;
	return NVME_ReadWriteLBA(namespace->drive, namespace->namespaceID, buf, lba, count, write);
}

bool NVME_Initialize(struct HAL_NVMEController *controller) {
	struct NVMEDrive *nvmeDriveInfo = ALLOC_OBJ(struct NVMEDrive);
	if (nvmeDriveInfo == NULL) {
		return false;
	}
	nvmeDriveInfo->adminCompletitionQueueHead = 0;
	nvmeDriveInfo->adminSubmissionQueueHead = 0;
	nvmeDriveInfo->completitionQueueHead = 0;
	nvmeDriveInfo->submissionQueueHead = 0;
	nvmeDriveInfo->controller = controller;
	nvmeDriveInfo->id = m_controllersDetected;
	nvmeDriveInfo->fallbackToPolling = true;
	m_controllersDetected++;
	Mutex_Initialize(&(nvmeDriveInfo->mutex));

	uintptr_t mappingPaddr = ALIGN_DOWN(controller->offset, HAL_VirtualMM_PageSize);
	size_t mappingSize = ALIGN_UP(controller->size + (controller->offset - mappingPaddr), HAL_VirtualMM_PageSize);
	uintptr_t mapping = HAL_VirtualMM_GetIOMapping(mappingPaddr, mappingSize, controller->disableCache);
	if (mapping == 0) {
		KernelLog_WarnMsg("NVME Driver", "Failed to map BAR0");
		return false;
	}
	VOLATILE uint32_t *bar0 = (VOLATILE uint32_t *)(mapping + (controller->offset - mappingPaddr));
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
	size_t submissionQueueSize =
		ALIGN_UP(sizeof(union NVME_SQEntry) * NVME_SUBMISSION_QUEUE_SIZE, HAL_VirtualMM_PageSize);
	union NVME_SQEntry *adminSubmissionQueue = (union NVME_SQEntry *)Heap_AllocateMemory(submissionQueueSize);
	union NVME_SQEntry *submissionQueue = (union NVME_SQEntry *)Heap_AllocateMemory(submissionQueueSize);

	size_t completitionQueueSize =
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

	uintptr_t adminSubmissionQueuePhysical = (uintptr_t)adminSubmissionQueue - HAL_VirtualMM_KernelMappingBase;
	uintptr_t adminCompletitionQueuePhysical = (uintptr_t)adminCompletitionQueue - HAL_VirtualMM_KernelMappingBase;
	uintptr_t submissionQueuePhysical = (uintptr_t)submissionQueue - HAL_VirtualMM_KernelMappingBase;
	uintptr_t completitionQueuePhysical = (uintptr_t)completitionQueue - HAL_VirtualMM_KernelMappingBase;

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

	size_t doorbellSize = 1 << (2 + cap.dstrd);
	uintptr_t doorbellsBase = (uint32_t)bar0 + 0x1000;
	nvmeDriveInfo->adminSubmissionDoorbell = (VOLATILE uint16_t *)(doorbellsBase + doorbellSize * 0);
	nvmeDriveInfo->adminCompletitionDoorbell = (VOLATILE uint16_t *)(doorbellsBase + doorbellSize * 1);
	nvmeDriveInfo->submissionDoorbell = (VOLATILE uint16_t *)(doorbellsBase + doorbellSize * 2);
	nvmeDriveInfo->completitionDoorbell = (VOLATILE uint16_t *)(doorbellsBase + doorbellSize * 3);

	union NVME_SQEntry identifyCommand = {0};
	identifyCommand.identify.opcode = NVME_IDENTIFY;
	identifyCommand.identify.nsid = 0;
	identifyCommand.identify.cns = 1;
	struct NVME_IdentifyInfo *identifyInfo = ALLOC_OBJ(struct NVME_IdentifyInfo);
	if (identifyInfo == NULL) {
		KernelLog_WarnMsg("NVME Driver", "Failed to allocate identify info object");
		return false;
	}
	identifyCommand.identify.prp1 = (uintptr_t)identifyInfo - HAL_VirtualMM_KernelMappingBase;
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

	size_t namespacesCount = identifyInfo->nn;
	KernelLog_InfoMsg("NVME Driver", "NVME Drive namespaces count: %u", identifyInfo->nn);
	struct nvme_drive_namespace *namespaces =
		(struct nvme_drive_namespace *)Heap_AllocateMemory(sizeof(struct nvme_drive_namespace) * namespacesCount);
	if (namespaces == NULL) {
		KernelLog_WarnMsg("NVME Driver", "Failed to allocate namespaces objects");
		return false;
	}
	nvmeDriveInfo->namespaces = namespaces;

	size_t maxSize = 1 << (12 + nvmeDriveInfo->identifyInfo->mdts);
	if (nvmeDriveInfo->identifyInfo->mdts > NVME_MAX_MDTS || maxSize == 4096) {
		maxSize = 1 << (12 + NVME_MAX_MDTS);
	}
	KernelLog_InfoMsg("NVME Driver", "Max transfer size: %u", maxSize);

	uint64_t *prps = Heap_AllocateMemory((maxSize / HAL_VirtualMM_PageSize) * 8);
	if (prps == NULL) {
		KernelLog_WarnMsg("NVME Driver", "Failed to allocate PRP list");
		return false;
	}

	nvmeDriveInfo->prps = prps;

	for (size_t i = 0; i < namespacesCount; ++i) {
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
		getNamespaceInfoCommand.identify.prp1 = (uintptr_t) namespace->info - HAL_VirtualMM_KernelMappingBase;
		if (NVME_ExecuteAdminCmd(nvmeDriveInfo, getNamespaceInfoCommand) != 0) {
			KernelLog_WarnMsg("NVME Driver", "Failed to read namespace info");
			return false;
		}
		KernelLog_InfoMsg("NVME Driver", "Successfully read NVME namespace info");

		uint8_t formatUsed = namespace->info->flbas & 0b11;
		size_t blockSize = 1 << namespace->info->lbaf[formatUsed].ds;
		size_t transferBlocksMax = maxSize / blockSize;

		namespace->block_size = blockSize;
		namespace->transferBlocksMax = transferBlocksMax;
		namespace->blocks_count = namespace->info->nsze;

		namespace->cache.sectorSize = namespace->block_size;
		namespace->cache.maxRWSectorsCount = namespace->transferBlocksMax;
		namespace->cache.sectorsCount = namespace->blocks_count;
		namespace->cache.rw_lba = NVME_NamespaceReadWriteLBA;
		namespace->cache.ctx = (void *)namespace;
		memset(namespace->cache.name, 0, 256);
		sprintf("nvme%un%u\0", namespace->cache.name, 256, nvmeDriveInfo->id, i + 1);
		namespace->cache.partitioningScheme = STORAGE_P_NUMERIC_PART_NAMING;
		if (!Storage_Init(&(namespace->cache))) {
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