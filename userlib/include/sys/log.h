#ifndef __CPL1_LIBC_LOG_H_INCLUDED__
#define __CPL1_LIBC_LOG_H_INCLUDED__

void Log_InitDoneMsg(const char *mod);
void Log_OkMsg(const char *mod, const char *fmt, ...);
void Log_WarnMsg(const char *mod, const char *fmt, ...);
void Log_ErrorMsg(const char *mod, const char *fmt, ...);
void Log_InfoMsg(const char *mod, const char *fmt, ...);
void Log_Print(const char *fmt, ...);

#endif
