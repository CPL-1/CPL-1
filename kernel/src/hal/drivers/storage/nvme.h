#ifndef __HAL_NVME_H_INCLUDED__
#define __HAL_NVME_H_INCLUDED__

#include <hal/proc/isrhandler.h>

struct hal_nvme_controller {
	void *ctx;
	uintptr_t offset;
	size_t size;
	bool disable_cache;
	void (*wait_for_event)(void *ctx);
	bool (*event_init)(void *ctx);
};

#endif