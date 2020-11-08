#include <drivers/pci.h>
#include <drivers/storage/nvme.h>
#include <drivers/storage/storage.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/virt.h>
#include <utils.h>

#define NVME_SUBMISSION_QUEUE_SIZE 2
#define NVME_COMPLETITION_QUEUE_SIZE 2

struct nvme_cap_register {
	uint32_t mqes : 16;
	uint32_t cqr : 1;
	uint32_t ams_is_weighted_rr_supported : 1;
	uint32_t ams_vendor_specific : 1;
	uint32_t : 5;
	uint32_t to : 8;
	uint32_t dstrd : 4;
	uint32_t nssrs : 1;
	uint32_t css_is_nvm_supported : 1;
	uint32_t : 6;
	uint32_t css_is_only_admin_supported : 1;
	uint32_t bps : 1;
	uint32_t : 2;
	uint32_t mpsmin : 4;
	uint32_t mpsmax : 4;
	uint32_t pmrs : 1;
	uint32_t cmbs : 1;
	uint32_t : 6;
} packed_align(8);

struct nvme_cc_register {
	uint32_t en : 1;
	uint32_t : 3;
	uint32_t css : 3;
	uint32_t mps : 4;
	uint32_t ams : 3;
	uint32_t shn : 2;
	uint32_t iosqes : 4;
	uint32_t iocqes : 4;
	uint32_t : 8;
} packed_align(4);

struct nvme_csts_register {
	uint32_t rdy : 1;
	uint32_t cfs : 1;
	uint32_t shst : 2;
	uint32_t nssro : 1;
	uint32_t pp : 1;
	uint32_t : 26;
} packed_align(4);

struct nvme_aqa_register {
	uint32_t asqs : 12;
	uint32_t : 4;
	uint32_t acqs : 12;
	uint32_t : 4;
} packed_align(4);

struct nvme_asq_register {
	uint64_t : 12;
	uint64_t page : 52;
} packed_align(8);

struct nvme_acq_register {
	uint64_t : 12;
	uint64_t page : 52;
} packed_align(8);

static struct nvme_cap_register nvme_read_cap_register(
    volatile uint32_t *bar0) {
	struct nvme_cap_register result = {0};
	uint64_t *as_pointer = (uint64_t *)&result;
	*as_pointer = ((uint64_t)bar0[1] << 32ULL) | (uint64_t)bar0[0];
	return result;
}

static struct nvme_cc_register nvme_read_cc_register(volatile uint32_t *bar0) {
	struct nvme_cc_register result = {0};
	uint32_t *as_pointer = (uint32_t *)&result;
	*as_pointer = bar0[5];
	return result;
}

static struct nvme_csts_register nvme_read_csts_register(
    volatile uint32_t *bar0) {
	struct nvme_csts_register result = {0};
	uint32_t *as_pointer = (uint32_t *)&result;
	*as_pointer = bar0[7];
	return result;
}

static struct nvme_aqa_register nvme_read_aqa_register(
    volatile uint32_t *bar0) {
	struct nvme_aqa_register result = {0};
	uint32_t *as_pointer = (uint32_t *)&result;
	*as_pointer = bar0[9];
	return result;
}

static struct nvme_asq_register nvme_read_asq_register(
    volatile uint32_t *bar0) {
	struct nvme_asq_register result = {0};
	uint64_t *as_pointer = (uint64_t *)&result;
	*as_pointer = ((uint64_t)bar0[11] << 32ULL) | (uint64_t)bar0[10];
	return result;
}

static struct nvme_acq_register nvme_read_acq_register(
    volatile uint32_t *bar0) {
	struct nvme_acq_register result = {0};
	uint64_t *as_pointer = (uint64_t *)&result;
	*as_pointer = ((uint64_t)bar0[13] << 32ULL) | (uint64_t)bar0[12];
	return result;
}

static void nvme_write_cc_register(volatile uint32_t *bar0,
                                   struct nvme_cc_register cc) {
	uint32_t *as_pointer = (uint32_t *)&cc;
	bar0[5] = *as_pointer;
}

unused static void nvme_write_csts_register(volatile uint32_t *bar0,
                                            struct nvme_csts_register csts) {
	uint32_t *as_pointer = (uint32_t *)&csts;
	bar0[7] = *as_pointer;
}
static void nvme_write_aqa_register(volatile uint32_t *bar0,
                                    struct nvme_aqa_register aqa) {
	uint32_t *as_pointer = (uint32_t *)&aqa;
	bar0[9] = *as_pointer;
}

static void nvme_write_asq_register(volatile uint32_t *bar0,
                                    struct nvme_asq_register cap) {
	uint64_t *as_pointer = (uint64_t *)&cap;
	bar0[10] = (uint32_t)(*as_pointer & 0xFFFFFFFF);
	bar0[11] = (uint32_t)((*as_pointer >> 32ULL) & 0xFFFFFFFF);
}

static void nvme_write_acq_register(volatile uint32_t *bar0,
                                    struct nvme_acq_register cap) {
	uint64_t *as_pointer = (uint64_t *)&cap;
	bar0[12] = (uint32_t)(*as_pointer & 0xFFFFFFFF);
	bar0[13] = (uint32_t)((*as_pointer >> 32ULL) & 0xFFFFFFFF);
}

enum nvme_admin_opcode {
	NVME_CREATE_SUBMISSION_QUEUE = 0x01,
	NVME_CREATE_COMPLETITION_QUEUE = 0x05,
	NVME_IDENTIFY = 0x06,
	NVME_SET_FEATURES = 0x09,
	NVME_GET_FEATURES = 0x0a
};

enum nvme_io_opcode { NVME_CMD_WRITE = 0x01, NVME_CMD_READ = 0x02 };

union nvme_sq_entry {
	struct {
		uint8_t opcode;
		uint8_t flags;
		uint16_t command_id;
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
	} packed identify;
} packed;

struct nvme_cq_entry {
	uint32_t command_specific;
	uint32_t reserved;
	uint16_t submission_queue_head_pointer;
	uint16_t submission_queue_identifier;
	uint16_t command_identifier;
	uint16_t phase_bit : 1;
	uint16_t status : 15;
} packed;

struct nvme_drive {
	volatile uint32_t *bar0;
	union nvme_sq_entry *admin_submission_queue;
	volatile struct nvme_cq_entry *admin_completition_queue;
	union nvme_sq_entry *submission_queue;
	volatile struct nvme_cq_entry *completition_queue;
	volatile uint16_t *admin_submission_doorbell;
	volatile uint16_t *admin_completition_doorbell;
	volatile uint16_t *submission_doorbell;
	volatile uint16_t *completition_doorbell;
	size_t admin_submission_queue_head;
	size_t admin_completition_queue_head;
	size_t submission_queue_head;
	size_t completition_queue_head;
	bool admin_phase_bit;
	bool phase_bit;
};

uint16_t nvme_execute_admin_cmd(struct nvme_drive *drive,
                                union nvme_sq_entry command) {
	drive->admin_submission_queue[drive->admin_submission_queue_head] = command;
	drive->admin_submission_queue_head++;
	if (drive->admin_submission_queue_head == NVME_SUBMISSION_QUEUE_SIZE) {
		drive->admin_submission_queue_head = 0;
	}
	volatile struct nvme_cq_entry *completition_entry =
	    drive->admin_completition_queue + drive->admin_completition_queue_head;
	completition_entry->phase_bit = drive->admin_phase_bit;

	*(drive->admin_submission_doorbell) = drive->admin_submission_queue_head;

	while (completition_entry->phase_bit == drive->admin_phase_bit) {
		struct nvme_csts_register csts;
		csts = nvme_read_csts_register(drive->bar0);
		if (csts.cfs == 1) {
			kmsg_err("NVME Driver",
			         "Fatal error occured while executing admin command\n");
		}
		asm volatile("pause");
	}

	drive->admin_completition_queue_head++;
	if (drive->admin_completition_queue_head == NVME_COMPLETITION_QUEUE_SIZE) {
		drive->admin_completition_queue_head = 0;
		drive->admin_phase_bit = !(drive->admin_phase_bit);
	}

	*(drive->admin_completition_doorbell) =
	    drive->admin_completition_queue_head;
	return drive->admin_completition_queue->status;
}

static bool nvme_rw_sectors(unused struct storage_drive *drive,
                            unused uint64_t offset, unused uint8_t *buf,
                            unused size_t sectors_count, unused bool write) {
	unused struct nvme_drive *nvme_drive_info =
	    (struct nvme_drive *)(drive->private_info);
	return false;
}

struct storage_drive_ops nvme_drive_ops = {
    .rw_sectors = nvme_rw_sectors,
};

void nvme_init(struct pci_address addr) {
	struct nvme_drive *nvme_drive_info = ALLOC_OBJ(struct nvme_drive);
	if (nvme_drive_info == NULL) {
		kmsg_err("NVME Driver", "Failed to allocate NVME drive object");
	}
	nvme_drive_info->admin_completition_queue_head = 0;
	nvme_drive_info->admin_submission_queue_head = 0;
	nvme_drive_info->completition_queue_head = 0;
	nvme_drive_info->submission_queue_head = 0;

	struct storage_drive *drive_info = ALLOC_OBJ(struct storage_drive);
	if (drive_info == NULL) {
		kmsg_err("NVME Driver", "Failed to allocate drive object");
	}

	drive_info->private_info = nvme_drive_info;
	drive_info->ops = &(nvme_drive_ops);
	struct pci_bar bar;
	if (!pci_read_bar(addr, 0, &bar)) {
		kmsg_err("NVME Driver", "Failed to read BAR0");
	}
	if (bar.is_mmio == false) {
		kmsg_err("NVME Driver", "BAR0 doesn't support MMIO");
	}
	printf("         NVME BAR0 MMIO base at 0x%p\n", bar.address);
	pci_enable_bus_mastering(addr);

	uint32_t mapping_paddr = ALIGN_DOWN(bar.address, PAGE_SIZE);
	uint32_t mapping_size =
	    ALIGN_UP(bar.size + (bar.address - mapping_paddr), PAGE_SIZE);
	uint32_t mapping =
	    virt_get_io_mapping(mapping_paddr, mapping_size, bar.disable_cache);
	if (mapping == 0) {
		kmsg_err("NVME Driver", "Failed to map BAR0");
	}
	volatile uint32_t *bar0 =
	    (volatile uint32_t *)(mapping + (bar.address - mapping_paddr));
	nvme_drive_info->bar0 = bar0;

	struct nvme_cc_register cc;
	struct nvme_csts_register csts;
	struct nvme_cap_register cap;
	struct nvme_aqa_register aqa;
	struct nvme_asq_register asq;
	struct nvme_acq_register acq;

	cc = nvme_read_cc_register(bar0);
	cc.en = 0;
	nvme_write_cc_register(bar0, cc);
	csts = nvme_read_csts_register(bar0);
	while (csts.rdy != 0) {
		asm volatile("pause");
		csts = nvme_read_csts_register(bar0);
	}
	kmsg_log("NVME Driver", "NVME Controller disabled");

	cap = nvme_read_cap_register(bar0);
	if (!cap.css_is_nvm_supported) {
		kmsg_err("NVME Driver",
		         "NVME controller doesn't support NVM command set");
	}
	if (cap.mpsmin > 0) {
		kmsg_err("NVME Driver",
		         "NVME controller doesn't support host page size");
	}

	size_t submission_queue_size = ALIGN_UP(
	    sizeof(union nvme_sq_entry) * NVME_SUBMISSION_QUEUE_SIZE, PAGE_SIZE);
	union nvme_sq_entry *admin_submission_queue =
	    (union nvme_sq_entry *)heap_alloc(submission_queue_size);
	union nvme_sq_entry *submission_queue =
	    (union nvme_sq_entry *)heap_alloc(submission_queue_size);

	size_t completition_queue_size = ALIGN_UP(
	    sizeof(struct nvme_cq_entry) * NVME_COMPLETITION_QUEUE_SIZE, PAGE_SIZE);
	struct nvme_cq_entry *admin_completition_queue =
	    (struct nvme_cq_entry *)heap_alloc(completition_queue_size);
	struct nvme_cq_entry *completition_queue =
	    (struct nvme_cq_entry *)heap_alloc(completition_queue_size);

	if (admin_submission_queue == NULL || admin_completition_queue == NULL ||
	    submission_queue == NULL || completition_queue == NULL) {
		kmsg_err("NVME Driver",
		         "Failed to allocate queues for NVME controller");
	}
	nvme_drive_info->admin_submission_queue = admin_submission_queue;
	nvme_drive_info->admin_completition_queue = admin_completition_queue;
	nvme_drive_info->submission_queue = submission_queue;
	nvme_drive_info->completition_queue = completition_queue;
	nvme_drive_info->admin_phase_bit = false;
	nvme_drive_info->phase_bit = false;
	memset(admin_submission_queue, 0, submission_queue_size);
	memset(admin_completition_queue, 0, completition_queue_size);
	memset(submission_queue, 0, submission_queue_size);
	memset(completition_queue, 0, completition_queue_size);
	kmsg_log("NVME Driver", "NVME controller admin queues allocated");

	uint32_t admin_submission_queue_physical =
	    (uint32_t)admin_submission_queue - KERNEL_MAPPING_BASE;
	uint32_t admin_completition_queue_physical =
	    (uint32_t)admin_completition_queue - KERNEL_MAPPING_BASE;

	asq = nvme_read_asq_register(bar0);
	acq = nvme_read_acq_register(bar0);
	asq.page = admin_submission_queue_physical / PAGE_SIZE;
	acq.page = admin_completition_queue_physical / PAGE_SIZE;
	nvme_write_asq_register(bar0, asq);
	nvme_write_acq_register(bar0, acq);

	aqa = nvme_read_aqa_register(bar0);
	aqa.acqs = 2;
	aqa.asqs = 2;
	nvme_write_aqa_register(bar0, aqa);

	cc = nvme_read_cc_register(bar0);
	cc.ams = 0;
	cc.mps = 0;
	cc.css = 0;
	cc.shn = 0;
	cc.iocqes = 4;
	cc.iosqes = 6;
	nvme_write_cc_register(bar0, cc);

	cc = nvme_read_cc_register(bar0);
	cc.en = 1;
	nvme_write_cc_register(bar0, cc);

	csts = nvme_read_csts_register(bar0);
	while (csts.rdy != 1) {
		if (csts.cfs != 0) {
			kmsg_err("NVME Driver", "Error while enabling controller");
		}
		asm volatile("pause");
		csts = nvme_read_csts_register(bar0);
	}
	kmsg_log("NVME driver", "NVME Controller reenabled");

	uint32_t doorbell_size = 1 << (2 + cap.dstrd);
	uint32_t doorbells_base = (uint32_t)bar0 + 0x1000;
	nvme_drive_info->admin_submission_doorbell =
	    (volatile uint16_t *)(doorbells_base + doorbell_size * 0);
	nvme_drive_info->admin_completition_doorbell =
	    (volatile uint16_t *)(doorbells_base + doorbell_size * 1);
	nvme_drive_info->submission_doorbell =
	    (volatile uint16_t *)(doorbells_base + doorbell_size * 2);
	nvme_drive_info->completition_doorbell =
	    (volatile uint16_t *)(doorbells_base + doorbell_size * 3);

	union nvme_sq_entry identify_command;
	memset(&identify_command, 0, sizeof(identify_command));
	identify_command.identify.opcode = NVME_IDENTIFY;
	identify_command.identify.nsid = 0;
	identify_command.identify.cns = 1;
	void *identify_info = heap_alloc(4096);
	if (identify_info == NULL) {
		kmsg_err("NVME driver", "Failed to allocate identify info object");
	}
	identify_command.identify.prp1 =
	    ((uint32_t)identify_info - KERNEL_MAPPING_BASE);
	identify_command.identify.prp2 = 0;
	nvme_execute_admin_cmd(nvme_drive_info, identify_command);
	kmsg_log("NVME Driver", "Identify command executed successfully");

	storage_add_drive(drive_info);
}