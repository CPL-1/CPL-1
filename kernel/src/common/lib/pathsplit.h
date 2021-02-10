#ifndef __PATH_SPLIT_H_INCLUDED__
#define __PATH_SPLIT_H_INCLUDED__

#include <common/misc/utils.h>

struct PathSplitter {
	char *copy;
	size_t size;
	size_t pos;
};

bool PathSplitter_Init(const char *str, struct PathSplitter *splitter);
const char *PathSplitter_Get(struct PathSplitter *splitter);
const char *PathSplitter_Advance(struct PathSplitter *splitter);
void PathSplitter_Dispose(struct PathSplitter *splitter);

#endif
