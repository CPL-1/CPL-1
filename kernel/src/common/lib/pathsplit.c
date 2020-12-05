#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>
#include <common/lib/pathsplit.h>

bool PathSplitter_Init(const char *str, struct PathSplitter *splitter) {
	splitter->pos = 0;
	splitter->size = strlen(str);
	splitter->copy = Heap_AllocateMemory(splitter->size + 1);
	if (splitter->copy == NULL) {
		return false;
	}
	for (size_t i = 0; i < splitter->size; ++i) {
		splitter->copy[i] = str[i];
		if (str[i] != splitter->copy[i]) {
			KernelLog_ErrorMsg("Path Split", "Bruh");
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

const char *PathSplitter_Get(struct PathSplitter *splitter) {
	if (splitter->pos >= splitter->size) {
		return NULL;
	}
	return splitter->copy + splitter->pos;
}

const char *PathSplitter_Advance(struct PathSplitter *splitter) {

	while (splitter->pos < splitter->size) {
		if (splitter->copy[splitter->pos] == '\0') {
			splitter->pos++;
			return PathSplitter_Get(splitter);
		}
		splitter->pos++;
	}
	return NULL;
}

void PathSplitter_Dispose(struct PathSplitter *splitter) {
	Heap_FreeMemory(splitter->copy, splitter->size + 1);
}
