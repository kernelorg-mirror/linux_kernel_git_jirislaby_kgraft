#ifndef __ASM_SH_FTRACE_H
#define __ASM_SH_FTRACE_H

#include <asm/fentry.h>

#ifdef CONFIG_FUNCTION_TRACER

#define FTRACE_SYSCALL_MAX	NR_syscalls

#ifndef __ASSEMBLY__

#ifdef CONFIG_DYNAMIC_FTRACE
#define CALL_ADDR		((long)(ftrace_call))
#define STUB_ADDR		((long)(ftrace_stub))
#define GRAPH_ADDR		((long)(ftrace_graph_call))
#define CALLER_ADDR		((long)(ftrace_caller))

#define MCOUNT_INSN_OFFSET	((STUB_ADDR - CALL_ADDR) - 4)
#define GRAPH_INSN_OFFSET	((CALLER_ADDR - GRAPH_ADDR) - 4)

#endif /* CONFIG_DYNAMIC_FTRACE */

#endif /* __ASSEMBLY__ */
#endif /* CONFIG_FUNCTION_TRACER */

#ifndef __ASSEMBLY__

/* arch/sh/kernel/return_address.c */
extern void *return_address(unsigned int);

#define HAVE_ARCH_CALLER_ADDR

#define CALLER_ADDR0 ((unsigned long)__builtin_return_address(0))
#define CALLER_ADDR1 ((unsigned long)return_address(1))
#define CALLER_ADDR2 ((unsigned long)return_address(2))
#define CALLER_ADDR3 ((unsigned long)return_address(3))
#define CALLER_ADDR4 ((unsigned long)return_address(4))
#define CALLER_ADDR5 ((unsigned long)return_address(5))
#define CALLER_ADDR6 ((unsigned long)return_address(6))

#endif /* __ASSEMBLY__ */

#endif /* __ASM_SH_FTRACE_H */
