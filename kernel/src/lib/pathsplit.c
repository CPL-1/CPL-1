#include <core/memory/heap.h>
#include <core/proc/intlock.h>
#include <lib/kmsg.h>
#include <lib/pathsplit.h>

bool path_splitter_init(const char *str, struct path_splitter *splitter) {
	splitter->pos = 0;
	splitter->size = strlen(str);
	splitter->copy = heap_alloc(splitter->size + 1);
	if (splitter->copy == NULL) {
		return false;
	}
	for (size_t i = 0; i < splitter->size; ++i) {
		splitter->copy[i] = str[i];
		if (str[i] != splitter->copy[i]) {
			kmsg_err("Path Split", "Bruh");
		}
	}
	for (size_t i = 0; i < splitter->size; ++i) {
		if (splitter->copy[i] == '/') {
			splitter->copy[i] = '\0';
		}
	}
	splitter->copy[splitter->size] = '\0';
	return true;
}

const char *path_splitter_get(struct path_splitter *splitter) {
	if (splitter->pos >= splitter->size) {
		return NULL;
	}
	return splitter->copy + splitter->pos;
}

const char *path_splitter_advance(struct path_splitter *splitter) {

	while (splitter->pos < splitter->size) {
		if (splitter->copy[splitter->pos] == '\0') {
			splitter->pos++;
			return path_splitter_get(splitter);
		}
		splitter->pos++;
	}
	return NULL;
}

void path_splitter_dispose(struct path_splitter *splitter) {
	heap_free(splitter->copy, splitter->size + 1);
}
