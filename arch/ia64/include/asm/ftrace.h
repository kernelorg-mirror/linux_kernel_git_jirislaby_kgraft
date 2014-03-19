#ifndef _ASM_IA64_FTRACE_H
#define _ASM_IA64_FTRACE_H

#include <asm/fentry.h>

#define FTRACE_ADDR (((struct fnptr *)ftrace_caller)->ip)

#endif /* _ASM_IA64_FTRACE_H */
