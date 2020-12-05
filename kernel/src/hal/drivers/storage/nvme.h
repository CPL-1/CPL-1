#ifndef __HAL_NVME_H_INCLUDED__
#define __HAL_NVME_H_INCLUDED__

#include <hal/proc/isrhandler.h>

struct hal_nvme_controller {
	void *ctx;
	uintptr_t offset;
	size_t size;
	bool disableCache;
	void (*waitForEvent)(void *ctx);
	bool (*initEvent)(void *ctx, void (*event_callback)(void *), void *private_ctx);
};

#endif