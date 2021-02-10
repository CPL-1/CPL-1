#include <common/core/memory/heap.h>
#include <hal/proc/isrhandler.h>

extern void i686_ISR_TemplateBegin();
extern void i686_ISR_TemplateEnd();
extern void i686_ISR_TemplateErrorCodeBegin();
extern void i686_ISR_TemplateErrorCodeEnd();

struct i686_ISR_ISRTemplate {
	char rsvd1[0x18];
	void *arg;
	char rsvd2;
	void *func;
} PACKED;

struct i686_ISR_ISRErrorCodeTemplate {
	char rsvd1[0x16];
	void *arg;
	char rsvd2;
	void *func;
} PACKED;

static size_t i686_ISR_GetTemplateSize() {
	return (uint32_t)i686_ISR_TemplateEnd - (uint32_t)i686_ISR_TemplateBegin;
}

static size_t i686_ISR_GetTemplateErrorCodeSize() {
	return (uint32_t)i686_ISR_TemplateErrorCodeEnd - (uint32_t)i686_ISR_TemplateErrorCodeBegin;
}

HAL_ISR_Handler i686_ISR_MakeNewISRHandler(void *func, void *arg, bool errorCode) {
	size_t templateSize = errorCode ? i686_ISR_GetTemplateErrorCodeSize() : i686_ISR_GetTemplateSize();
	void *newFunction = Heap_AllocateMemory(templateSize);
	if (newFunction == NULL) {
		return NULL;
	}
	memcpy(newFunction, (void *)(errorCode ? i686_ISR_TemplateErrorCodeBegin : i686_ISR_TemplateBegin), templateSize);
	if (errorCode) {
		struct i686_ISR_ISRErrorCodeTemplate *template = (struct i686_ISR_ISRErrorCodeTemplate *)newFunction;
		template->func = func;
		template->arg = arg;
	} else {
		struct i686_ISR_ISRTemplate *template = (struct i686_ISR_ISRTemplate *)newFunction;
		template->func = func;
		template->arg = arg;
	}
	return (HAL_ISR_Handler)newFunction;
}
