#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/ftrace.h>
#include <linux/fentry.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/list.h>
#include <trace/syscall.h>

#include <asm/ftrace.h>

#ifdef CONFIG_DYNAMIC_FTRACE
static const u32 ftrace_nop = 0x01000000;

int ftrace_make_call(struct fentry *rec, unsigned long addr)
{
	unsigned long ip = rec->ip;
	u32 old, new;

	old = ftrace_nop;
	new = fentry_call_replace(ip, addr);
	return fentry_modify_code(ip, old, new);
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned long ip = (unsigned long)(&ftrace_call);
	u32 old, new;

	old = *(u32 *) &ftrace_call;
	new = fentry_call_replace(ip, (unsigned long)func);
	return fentry_modify_code(ip, old, new);
}
#endif

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

#ifdef CONFIG_DYNAMIC_FTRACE
extern void ftrace_graph_call(void);

int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);
	u32 old, new;

	old = *(u32 *) &ftrace_graph_call;
	new = fentry_call_replace(ip, (unsigned long) &ftrace_graph_caller);
	return fentry_modify_code(ip, old, new);
}

int ftrace_disable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);
	u32 old, new;

	old = *(u32 *) &ftrace_graph_call;
	new = fentry_call_replace(ip, (unsigned long) &ftrace_stub);

	return fentry_modify_code(ip, old, new);
}

#endif /* !CONFIG_DYNAMIC_FTRACE */

/*
 * Hook the return address and push it in the stack of return addrs
 * in current thread info.
 */
unsigned long prepare_ftrace_return(unsigned long parent,
				    unsigned long self_addr,
				    unsigned long frame_pointer)
{
	unsigned long return_hooker = (unsigned long) &return_to_handler;
	struct ftrace_graph_ent trace;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		return parent + 8UL;

	if (ftrace_push_return_trace(parent, self_addr, &trace.depth,
				     frame_pointer) == -EBUSY)
		return parent + 8UL;

	trace.func = self_addr;

	/* Only trace if the calling function expects to */
	if (!ftrace_graph_entry(&trace)) {
		current->curr_ret_stack--;
		return parent + 8UL;
	}

	return return_hooker;
}
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */
