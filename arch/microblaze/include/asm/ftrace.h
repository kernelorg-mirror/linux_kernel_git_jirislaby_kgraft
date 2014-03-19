#ifndef _ASM_MICROBLAZE_FTRACE
#define _ASM_MICROBLAZE_FTRACE

#include <asm/fentry.h>

#ifdef CONFIG_FUNCTION_TRACER

#ifndef __ASSEMBLY__
extern void ftrace_call_graph(void);
#endif

#endif /* CONFIG_FUNCTION_TRACER */
#endif /* _ASM_MICROBLAZE_FTRACE */
