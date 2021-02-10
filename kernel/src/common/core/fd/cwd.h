#ifndef __CWD_H_INCLUDED__
#define __CWD_H_INCLUDED__

#include <common/misc/utils.h>

struct CWD_Info *CWD_MakeRootCwdInfo();
struct CWD_Info *CWD_ForkCwdInfo(struct CWD_Info *info);
bool CWD_ChangeDirectory(struct CWD_Info *info, const char *path);
int CWD_GetWorkingDirectoryPath(struct CWD_Info *info, char *buf, size_t length);
void CWD_DisposeInfo(struct CWD_Info *info);

#endif
