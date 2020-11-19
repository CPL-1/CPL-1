#ifndef __KMSG_H_INCLUDED__
#define __KMSG_H_INCLUDED__

#include <utils/utils.h>

void kmsg_init_done(const char *mod);
void kmsg_ok(const char *mode, const char *text);
void kmsg_warn(const char *mod, const char *text);
void kmsg_err(const char *mod, const char *text);
void kmsg_log(const char *mod, const char *text);

#endif
