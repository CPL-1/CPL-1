#ifndef __PATH_SPLIT_H_INCLUDED__
#define __PATH_SPLIT_H_INCLUDED__

#include <utils/utils.h>

struct path_splitter {
	char *copy;
	size_t size;
	size_t pos;
};

bool path_splitter_init(const char *str, struct path_splitter *splitter);
const char *path_splitter_get(struct path_splitter *splitter);
const char *path_splitter_advance(struct path_splitter *splitter);
void path_splitter_dispose(struct path_splitter *splitter);

#endif