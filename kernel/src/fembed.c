#include <fembed.h>
#include <memory/heap.h>

extern void fembed_template();
extern void fembed_template_end();

struct function_with_argument {
	char rsvd1[0x16];
	void *arg;
	char rsvd2;
	void *func;
} packed;

size_t fembed_size() {
	size_t template_size =
		(uint32_t)fembed_template_end - (uint32_t)fembed_template;
	return template_size;
}

void *fembed_make_irq_handler(void *func, void *arg) {
	struct function_with_argument *new_function =
		(struct function_with_argument *)heap_alloc(fembed_size());
	if (new_function == NULL) {
		return NULL;
	}
	memcpy(new_function, (void *)fembed_template, fembed_size());
	new_function->func = func;
	new_function->arg = arg;
	return (void *)new_function;
}