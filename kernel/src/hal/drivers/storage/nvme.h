#ifndef __HAL_NVME_H_INCLUDED__
#define __HAL_NVME_H_INCLUDED__

#include <hal/proc/isrhandler.h>

struct HAL_NVMEController {
	void *ctx;
	uintptr_t offset;
	size_t size;
	bool disableCache;
	void (*waitForEvent)(void *ctx);
	bool (*initEvent)(void *ctx, void (*eventCallback)(void *), void *private_ctx);
};

#endif