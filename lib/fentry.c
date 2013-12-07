/*
 * Infrastructure for profiling code inserted by 'gcc -pg'.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 * Copyright (C) 2004-2008 Ingo Molnar <mingo@redhat.com>
 *
 * Originally ported from the -rt patch by:
 *   Copyright (C) 2007 Arnaldo Carvalho de Melo <acme@redhat.com>
 *
 * Based on code in the latency_tracer, that is:
 *
 *  Copyright (C) 2004-2006 Ingo Molnar
 *  Copyright (C) 2004 Nadia Yvette Chambers
 */

#include <linux/fentry.h>
#include <linux/ftrace.h>
#include <linux/hrtimer.h>
#include <linux/mm.h>
#include <linux/memory.h> /* text_mutex */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sort.h>

#define ENTRY_SIZE		sizeof(struct fentry)
#define ENTRIES_PER_PAGE	(PAGE_SIZE / ENTRY_SIZE)

DEFINE_MUTEX(fentry_lock);
struct fentry_page *fentry_pages_start;
unsigned long fentry_count;
bool fentry_enabled;

static struct fentry_page *fentry_pages;
static ktime_t fentry_update_time;

static int fentry_allocate_records(struct fentry_page *pg, int count)
{
	int order;
	int cnt;

	if (WARN_ON(!count))
		return -EINVAL;

	order = get_count_order(DIV_ROUND_UP(count, ENTRIES_PER_PAGE));

	/*
	 * We want to fill as much as possible. No more than a page
	 * may be empty.
	 */
	while ((PAGE_SIZE << order) / ENTRY_SIZE >= count + ENTRIES_PER_PAGE)
		order--;

 again:
	pg->records = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, order);

	if (!pg->records) {
		/* if we can't allocate this size, try something smaller */
		if (!order)
			return -ENOMEM;
		order >>= 1;
		goto again;
	}

	cnt = (PAGE_SIZE << order) / ENTRY_SIZE;
	pg->size = cnt;

	if (cnt > count)
		cnt = count;

	return cnt;
}

static struct fentry_page *
fentry_allocate_pages(unsigned long num_to_init)
{
	struct fentry_page *start_pg;
	struct fentry_page *pg;
	int order;
	int cnt;

	if (!num_to_init)
		return 0;

	start_pg = pg = kzalloc(sizeof(*pg), GFP_KERNEL);
	if (!pg)
		return NULL;

	/*
	 * Try to allocate as much as possible in one continues
	 * location that fills in all of the space. We want to
	 * waste as little space as possible.
	 */
	for (;;) {
		cnt = fentry_allocate_records(pg, num_to_init);
		if (cnt < 0)
			goto free_pages;

		num_to_init -= cnt;
		if (!num_to_init)
			break;

		pg->next = kzalloc(sizeof(*pg), GFP_KERNEL);
		if (!pg->next)
			goto free_pages;

		pg = pg->next;
	}

	return start_pg;

 free_pages:
	while (start_pg) {
		order = get_count_order(pg->size / ENTRIES_PER_PAGE);
		free_pages((unsigned long)pg->records, order);
		start_pg = pg->next;
		kfree(pg);
		pg = start_pg;
	}
	pr_info("ftrace: FAILED to allocate memory for functions\n");
	return NULL;
}

static int fentry_cmp_ips(const void *a, const void *b)
{
	const unsigned long *ipa = a;
	const unsigned long *ipb = b;

	if (*ipa > *ipb)
		return 1;
	if (*ipa < *ipb)
		return -1;
	return 0;
}

static void fentry_swap_ips(void *a, void *b, int size)
{
	unsigned long *ipa = a;
	unsigned long *ipb = b;
	unsigned long t;

	t = *ipa;
	*ipa = *ipb;
	*ipb = t;
}

static int
fentry_code_disable(struct module *mod, struct fentry *rec)
{
	unsigned long ip;
	int ret;

	ip = rec->ip;

	ret = fentry_make_nop(mod, rec, MCOUNT_ADDR);
	if (ret)
		pr_err("%s: cannot make nop at %pS\n", __func__, (void *)ip);

	return ret;
}

static int fentry_update_code(struct module *mod, struct fentry_page *new_pgs)
{
	struct fentry_page *pg;
	struct fentry *p;
	ktime_t start;
	unsigned long update_cnt = 0;
	int i, ret;

	start = ktime_get();

	for (pg = new_pgs; pg; pg = pg->next) {

		for (i = 0; i < pg->index; i++) {

			p = &pg->records[i];

			/*
			 * Do the initial record conversion from fentry jump
			 * to the NOP instructions.
			 */
			ret = fentry_code_disable(mod, p);
			if (ret)
				return ret;

			update_cnt++;
		}
	}

	ret = ftrace_init_install(mod, new_pgs);
	if (ret)
		return ret;

	fentry_update_time = ktime_sub(ktime_get(), start);
	fentry_count += update_cnt;

	return 0;
}

static int fentry_process_locs(struct module *mod,
			       unsigned long *start,
			       unsigned long *end)
{
	struct fentry_page *start_pg;
	struct fentry_page *pg;
	struct fentry *rec;
	unsigned long count;
	unsigned long *p;
	unsigned long addr;
	unsigned long flags = 0; /* Shut up gcc */
	int ret = -ENOMEM;

	count = end - start;

	if (!count)
		return 0;

	sort(start, count, sizeof(*start),
	     fentry_cmp_ips, fentry_swap_ips);

	start_pg = fentry_allocate_pages(count);
	if (!start_pg)
		return -ENOMEM;

	mutex_lock(&fentry_lock);

	/*
	 * Core and each module needs their own pages, as
	 * modules will free them when they are removed.
	 * Force a new page to be allocated for modules.
	 */
	if (!mod) {
		WARN_ON(fentry_pages || fentry_pages_start);
		/* First initialization */
		fentry_pages = fentry_pages_start = start_pg;
	} else {
		if (!fentry_pages)
			goto out;

		if (WARN_ON(fentry_pages->next)) {
			/* Hmm, we have free pages? */
			while (fentry_pages->next)
				fentry_pages = fentry_pages->next;
		}

		fentry_pages->next = start_pg;
	}

	p = start;
	pg = start_pg;
	while (p < end) {
		addr = fentry_call_adjust(*p++);
		/*
		 * Some architecture linkers will pad between
		 * the different mcount_loc sections of different
		 * object files to satisfy alignments.
		 * Skip any NULL pointers.
		 */
		if (!addr)
			continue;

		if (pg->index == pg->size) {
			/* We should have allocated enough */
			if (WARN_ON(!pg->next))
				break;
			pg = pg->next;
		}

		rec = &pg->records[pg->index++];
		rec->ip = addr;
	}

	/* We should have used all pages */
	WARN_ON(pg->next);

	/* Assign the last page to ftrace_pages */
	fentry_pages = pg;

	/*
	 * We only need to disable interrupts on start up
	 * because we are modifying code that an interrupt
	 * may execute, and the modification is not atomic.
	 * But for modules, nothing runs the code we modify
	 * until we are finished with it, and there's no
	 * reason to cause large interrupt latencies while we do it.
	 */
	if (!mod)
		local_irq_save(flags);
	ret = fentry_update_code(mod, start_pg);
	if (!mod)
		local_irq_restore(flags);
 out:
	mutex_unlock(&fentry_lock);

	return ret;
}

#ifdef CONFIG_MODULES

static void fentry_init_module(struct module *mod,
			       unsigned long *start, unsigned long *end)
{
	if (!fentry_enabled || start == end)
		return;
	if (fentry_process_locs(mod, start, end))
		fentry_enabled = false;
}

static int fentry_module_notify_enter(struct notifier_block *self,
				      unsigned long val, void *data)
{
	struct module *mod = data;

	if (val == MODULE_STATE_COMING)
		fentry_init_module(mod, mod->ftrace_callsites,
				   mod->ftrace_callsites +
				   mod->num_ftrace_callsites);
	return 0;
}

#define next_to_fentry_page(p) container_of(p, struct fentry_page, next)

void fentry_release_mod(struct module *mod)
{
	struct fentry *rec;
	struct fentry_page **last_pg;
	struct fentry_page *pg;
	int order;

	mutex_lock(&fentry_lock);

	/*
	 * Each module has its own fentry_pages, remove
	 * them from the list.
	 */
	last_pg = &fentry_pages_start;
	for (pg = fentry_pages_start; pg; pg = *last_pg) {
		rec = &pg->records[0];
		if (within_module_core(rec->ip, mod)) {
			/*
			 * As core pages are first, the first
			 * page should never be a module page.
			 */
			if (WARN_ON(pg == fentry_pages_start))
				goto out_unlock;

			/* Check if we are deleting the last page */
			if (pg == fentry_pages)
				fentry_pages = next_to_fentry_page(last_pg);

			*last_pg = pg->next;
			order = get_count_order(pg->size / ENTRIES_PER_PAGE);
			free_pages((unsigned long)pg->records, order);
			kfree(pg);
		} else
			last_pg = &pg->next;
	}
 out_unlock:
	mutex_unlock(&fentry_lock);
}

static int fentry_module_notify_exit(struct notifier_block *self,
				     unsigned long val, void *data)
{
	struct module *mod = data;

	if (val == MODULE_STATE_GOING)
		fentry_release_mod(mod);

	return 0;
}
#else
static int fentry_module_notify_enter(struct notifier_block *self,
				      unsigned long val, void *data)
{
	return 0;
}
static int fentry_module_notify_exit(struct notifier_block *self,
				     unsigned long val, void *data)
{
	return 0;
}
#endif /* CONFIG_MODULES */

struct notifier_block fentry_module_enter_nb = {
	.notifier_call = fentry_module_notify_enter,
	.priority = INT_MAX,	/* Run before anything that can use kprobes */
};

struct notifier_block fentry_module_exit_nb = {
	.notifier_call = fentry_module_notify_exit,
	.priority = INT_MIN,	/* Run after anything that can remove kprobes */
};

void __init fentry_init(void)
{
	extern unsigned long __start_mcount_loc[];
	extern unsigned long __stop_mcount_loc[];
	unsigned long count, flags;
	int ret;

	local_irq_save(flags);
	ret = fentry_dyn_arch_init();
	local_irq_restore(flags);
	if (ret)
		return;

	count = __stop_mcount_loc - __start_mcount_loc;
	if (!count) {
		pr_info("fentry: No functions to be traced?\n");
		return;
	}

	pr_info("fentry: allocating %ld entries in %ld pages\n",
		count, count / ENTRIES_PER_PAGE + 1);

	ret = fentry_process_locs(NULL,
				  __start_mcount_loc,
				  __stop_mcount_loc);
	if (ret)
		return;

	ret = register_module_notifier(&fentry_module_enter_nb);
	if (ret)
		pr_warning("Failed to register fentry module enter notifier\n");

	ret = register_module_notifier(&fentry_module_exit_nb);
	if (ret)
		pr_warning("Failed to register fentry module exit notifier\n");

	/* now, let's BUG as no more fentries should be called */
	mutex_lock(&text_mutex);
	fentry_put_BUG();
	mutex_unlock(&text_mutex);

	fentry_enabled = true;
}
