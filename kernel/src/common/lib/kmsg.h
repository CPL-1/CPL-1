#ifndef __KMSG_H_INCLUDED__
#define __KMSG_H_INCLUDED__

#include <common/misc/utils.h>

void KernelLog_InitDoneMsg(const char *mod);
void KernelLog_OkMsg(const char *mode, const char *fmt, ...);
void KernelLog_WarnMsg(const char *mod, const char *fmt, ...);
void KernelLog_ErrorMsg(const char *mod, const char *fmt, ...);
void KernelLog_InfoMsg(const char *mod, const char *fmt, ...);
void KernelLog_Print(const char *fmt, ...);

#endif
