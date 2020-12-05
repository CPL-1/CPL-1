#include <common/core/memory/heap.h>
#include <hal/proc/isrhandler.h>

extern void i686_ISR_TemplateBegin();
extern void i686_ISR_TemplateEnd();

struct i686_ISR_ISRTemplate {
	char rsvd1[0x16];
	void *arg;
	char rsvd2;
	void *func;
} PACKED;

static size_t i686_ISR_GetTemplateSize() {
	return (uint32_t)i686_ISR_TemplateEnd - (uint32_t)i686_ISR_TemplateBegin;
}

HAL_ISR_Handler i686_ISR_MakeNewISRHandler(void *func, void *arg) {
	struct i686_ISR_ISRTemplate *newFunction =
		(struct i686_ISR_ISRTemplate *)Heap_AllocateMemory(i686_ISR_GetTemplateSize());
	if (newFunction == NULL) {
		return NULL;
	}
	memcpy(newFunction, (void *)i686_ISR_TemplateBegin, i686_ISR_GetTemplateSize());
	newFunction->func = func;
	newFunction->arg = arg;
	return (HAL_ISR_Handler)newFunction;
}