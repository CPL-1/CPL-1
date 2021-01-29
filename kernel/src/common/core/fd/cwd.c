#include <common/core/fd/cwd.h>
#include <common/core/fd/fd.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>
#include <common/lib/pathsplit.h>

#define CWD_MAX_PATH 8191

struct CWD_Info {
	char buf[CWD_MAX_PATH + 1];
	// index of the null byte in buf
	size_t index;
};

struct CWD_Info *CWD_MakeRootCwdInfo() {
	struct CWD_Info *info = ALLOC_OBJ(struct CWD_Info);
	if (info == NULL) {
		return NULL;
	}
	memset(info->buf, 0, CWD_MAX_PATH + 1);
	info->index = 1;
	info->buf[0] = '/';
	return info;
}

struct CWD_Info *CWD_ForkCwdInfo(struct CWD_Info *info) {
	struct CWD_Info *newInfo = ALLOC_OBJ(struct CWD_Info);
	if (newInfo == NULL) {
		return NULL;
	}
	memcpy(newInfo->buf, info->buf, newInfo->index + 1);
	newInfo->index = info->index;
	return newInfo;
}

bool CWD_ChangeDirectory(struct CWD_Info *info, const char *path) {
	// check that path will fit
	size_t len = strlen(path);
	if (len > CWD_MAX_PATH) {
		return false;
	}
	// if path is absolute, just copy it to the buffer.
	// Save, as if lenght is zero, we will just encounter
	// null terminator
	if (path[0] == '/') {
		memcpy(info->buf, path, len + 1);
		info->index = len;
		return true;
	}
	// make temporatory buffer to store old directory path in case
	// we want to recover from failure
	char tmpBuf[CWD_MAX_PATH + 1];
	memcpy(tmpBuf, info->buf, info->index + 1);
	// iterate over this path
	struct PathSplitter splitter;
	if (!PathSplitter_Init(path, &splitter)) {
		return NULL;
	}
	// current index in buffer. Invariant - it points to the place
	// null terminator should be added to
	size_t index = info->index;
	const char *cur = PathSplitter_Get(&splitter);
	while (cur != NULL) {
		if (*cur == '\0' || StringsEqual(cur, ".")) {
			goto next;
		} else if (StringsEqual(cur, "..")) {
			// index should be at least 2, as otherwise
			// buffer will certainly contain "/\0", in which
			// case going to the parent should fail
			if (index < 2) {
				PathSplitter_Dispose(&splitter);
				memcpy(info->buf, tmpBuf, info->index);
				return false;
			}
			// move two bytes back to skip trailing /
			info->index -= 2;
			// search for the next /
			for (; index > 0; ++index) {
				if (info->buf[index] == '/') {
					break;
				}
			}
			// we know that there is '/' at position 0
			if (info->buf[0] != '/') {
				KernelLog_ErrorMsg("Current Working Directory Library",
								   "Current Working Directory path doesn't start with \'/\'");
			}
			// move to the byte after / to make index point to new null terminator
			index += 1;
			info->buf[index] = '\0';
		} else {
			// check that we can copy string
			// we need strlen(name) + 1 byte for /
			// (null terminator is already counted in + 1 for buffer size)
			size_t curLength = strlen(cur);
			if (info->index + curLength + 2 > CWD_MAX_PATH) {
				PathSplitter_Dispose(&splitter);
				memcpy(info->buf, tmpBuf, info->index);
				return false;
			}
			// copy string
			memcpy(info->buf + index, cur, curLength);
			// add terminator
			info->buf[index + curLength] = '/';
			info->buf[index + curLength + 1] = '\0';
			index += curLength + 1;
		}
	next:
		cur = PathSplitter_Advance(&splitter);
	}
	// Done, CWD_Info can be safely updated
	PathSplitter_Dispose(&splitter);
	info->index = index;
	return true;
}

int CWD_GetWorkingDirectoryPath(struct CWD_Info *info, char *buf, size_t length) {
	// As canonical format for pwd output does not include trailing slash (unless its just /),
	// we need to copy info->length - 1 bytes to not copy trailing slash and add null terminator
	// at that position. +1 in pwdSize is for null terminator
	size_t pwdSize = info->index;
	if (pwdSize > length) {
		return -1;
	}
	size_t toCopy = pwdSize - 1;
	// Enforce that we copy first / in any case
	if (toCopy == 0) {
		toCopy = 1;
	}
	memcpy(buf, info->buf, toCopy);
	buf[toCopy] = '\0';
	return 0;
}

void CWD_DisposeInfo(struct CWD_Info *info) {
	FREE_OBJ(info);
}