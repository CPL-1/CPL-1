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
	info->buf[1] = '\0';
	return info;
}

struct CWD_Info *CWD_ForkCwdInfo(struct CWD_Info *info) {
	struct CWD_Info *newInfo = ALLOC_OBJ(struct CWD_Info);
	if (newInfo == NULL) {
		return NULL;
	}
	memcpy(newInfo->buf, info->buf, info->index + 1);
	newInfo->index = info->index;
	return newInfo;
}

bool CWD_ChangeDirectory(struct CWD_Info *info, const char *path) {
	// check that path will fit
	size_t len = strlen(path);
	if (len + info->index >= CWD_MAX_PATH) {
		return false;
	}
	// copy path
	memcpy(info->buf + info->index, path, len);
	info->index = info->index + len;
	info->buf[info->index] = '/';
	info->buf[info->index + 1] = '\0';
	info->index++;
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