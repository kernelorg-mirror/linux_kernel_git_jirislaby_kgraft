/*
 * kGraft Online Kernel Patching
 *
 *  Copyright (c) 2013-2014 SUSE
 *   Authors: Jiri Kosina
 *	      Vojtech Pavlik
 *	      Jiri Slaby
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/bitmap.h>
#include <linux/bug.h>
#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/kgraft.h>
#include <linux/livepatch.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

static int kgr_patch_code(struct kgr_patch_fun *patch_fun, bool final,
		bool revert);
static void kgr_work_fn(struct work_struct *work);

static struct workqueue_struct *kgr_wq;
static DECLARE_DELAYED_WORK(kgr_work, kgr_work_fn);
static DEFINE_MUTEX(kgr_in_progress_lock);
bool kgr_in_progress;
static bool kgr_initialized;
static struct kgr_patch *kgr_patch;
static bool kgr_revert;
/*
 * Setting the per-process flag and stub instantiation has to be performed
 * "atomically", otherwise the flag might get cleared and old function called
 * during the race window.
 *
 * kgr_immutable is an atomic flag which signals whether we are in the
 * actual race window and lets the stub take a proper action (reset the
 * 'in progress' state)
 */
static DECLARE_BITMAP(kgr_immutable, 1);

/*
 * The stub needs to modify the RIP value stored in struct pt_regs
 * so that ftrace redirects the execution properly.
 *
 * Stubs have to be labeled with notrace to prevent recursion loop in ftrace.
 */
static notrace void kgr_stub_fast(unsigned long ip, unsigned long parent_ip,
		struct ftrace_ops *ops, struct pt_regs *regs)
{
	struct kgr_patch_fun *p = ops->private;

	klp_arch_set_pc(regs, (unsigned long)p->new_fun);
}

static notrace void kgr_stub_slow(unsigned long ip, unsigned long parent_ip,
		struct ftrace_ops *ops, struct pt_regs *regs)
{
	struct kgr_patch_fun *p = ops->private;
	bool go_new;

	if (test_bit(0, kgr_immutable)) {
		klp_kgraft_mark_task_in_progress(current);
		go_new = false;
	} else {
		rmb(); /* test_bit before kgr_mark_task_in_progress */
		go_new = !klp_kgraft_task_in_progress(current);
	}

	if (p->state == KGR_PATCH_REVERT_SLOW)
		go_new = !go_new;

	/* Redirect the function unless we continue with the original one. */
	if (go_new)
		klp_arch_set_pc(regs, (unsigned long)p->new_fun);
}

static int kgr_ftrace_enable(struct kgr_patch_fun *pf, struct ftrace_ops *fops)
{
	int ret;

	ret = ftrace_set_filter_ip(fops, pf->loc_old, 0, 0);
	if (ret)
		return ret;

	ret = register_ftrace_function(fops);
	if (ret)
		ftrace_set_filter_ip(fops, pf->loc_old, 1, 0);

	return ret;
}

static int kgr_ftrace_disable(struct kgr_patch_fun *pf, struct ftrace_ops *fops)
{
	int ret;

	ret = unregister_ftrace_function(fops);
	if (ret)
		return ret;

	ret = ftrace_set_filter_ip(fops, pf->loc_old, 1, 0);
	if (ret)
		register_ftrace_function(fops);

	return ret;
}

static bool kgr_still_patching(void)
{
	struct task_struct *p, *t;
	bool failed = false;

	read_lock(&tasklist_lock);
	for_each_process_thread(p, t) {
		if (klp_kgraft_task_in_progress(t)) {
			failed = true;
			goto unlock;
		}
	}
unlock:
	read_unlock(&tasklist_lock);
	return failed;
}

static void kgr_finalize(void)
{
	struct kgr_patch_fun *patch_fun;
	int ret;

	mutex_lock(&kgr_in_progress_lock);

	kgr_for_each_patch_fun(kgr_patch, patch_fun) {
		ret = kgr_patch_code(patch_fun, true, kgr_revert);

		if (ret < 0) {
			pr_err("kgr: finalization for %s failed (%d). System in inconsistent state with no way out.\n",
				patch_fun->name, ret);
			BUG();
		}
	}

	if (kgr_revert)
		module_put(kgr_patch->owner);

	kgr_patch = NULL;
	kgr_in_progress = false;

	pr_info("kgr succeeded\n");

	mutex_unlock(&kgr_in_progress_lock);
}

static void kgr_work_fn(struct work_struct *work)
{
	static bool printed = false;

	if (kgr_still_patching()) {
		if (!printed) {
			pr_info("kgr still in progress after timeout, will keep"
					" trying every %d seconds\n",
				KGR_TIMEOUT);
			printed = true;
		}
		/* recheck again later */
		queue_delayed_work(kgr_wq, &kgr_work, KGR_TIMEOUT * HZ);
		return;
	}

	/*
	 * victory, patching finished, put everything back in shape
	 * with as less performance impact as possible again
	 */
	kgr_finalize();
	printed = false;
}

static void kgr_handle_processes(void)
{
	struct task_struct *p, *t;

	read_lock(&tasklist_lock);
	for_each_process_thread(p, t) {
		klp_kgraft_mark_task_in_progress(t);
	}
	read_unlock(&tasklist_lock);
}

static void kgr_wakeup_kthreads(void)
{
	struct task_struct *p, *t;

	read_lock(&tasklist_lock);
	for_each_process_thread(p, t) {
		/*
		 * Wake up kthreads, they will clean the progress flag.
		 *
		 * There is a small race here. We could see TIF_KGR_IN_PROGRESS
		 * set and decide to wake up a kthread. Meanwhile the kthread
		 * could migrate itself and the waking up would be meaningless.
		 * It is not serious though.
		 */
		if ((t->flags & PF_KTHREAD) &&
				klp_kgraft_task_in_progress(t)) {
			/*
			 * this is incorrect for kthreads waiting still for
			 * their first wake_up.
			 */
			wake_up_process(t);
		}
	}
	read_unlock(&tasklist_lock);
}

static unsigned long kgr_get_function_address(const struct kgr_patch_fun *pf)
{
	unsigned long orig_addr;
	const char *check_name;
	char check_buf[KSYM_SYMBOL_LEN];

	orig_addr = kallsyms_lookup_name(pf->name);
	if (!orig_addr) {
		if (pf->abort_if_missing)
			pr_err("kgr: function %s not resolved\n", pf->name);
		return -ENOENT;
	}

	check_name = kallsyms_lookup(orig_addr, NULL, NULL, NULL, check_buf);
	if (strcmp(check_name, pf->name)) {
		pr_err("kgr: we got out of bounds the intended function (%s -> %s)\n",
				pf->name, check_name);
		return -EINVAL;
	}

	return orig_addr;
}

static int kgr_switch_fops(struct kgr_patch_fun *patch_fun,
		struct ftrace_ops *new_fops, struct ftrace_ops *unreg_fops)
{
	int err;

	if (new_fops) {
		err = kgr_ftrace_enable(patch_fun, new_fops);
		if (err) {
			pr_err("kgr: cannot enable ftrace function for %s (%lx, %d)\n",
				patch_fun->name, patch_fun->loc_old, err);
			return err;
		}
	}

	/*
	 * Get rid of the other stub. Having two stubs in the interim is fine.
	 * The first one registered always "wins", as it'll be dragged last from
	 * the ftrace hashtable. The redirected RIP however currently points to
	 * the same function in both stubs.
	 */
	if (unreg_fops) {
		err = kgr_ftrace_disable(patch_fun, unreg_fops);
		if (err) {
			pr_err("kgr: disabling ftrace function for %s failed (%d)\n",
				patch_fun->name, err);
			/*
			 * In case of failure we do not know which state we are
			 * in. There is something wrong going on in kGraft of
			 * ftrace, so better BUG.
			 */
			BUG();
		}
	}

	return 0;
}

static int kgr_init_ftrace_ops(struct kgr_patch_fun *patch_fun)
{
	struct ftrace_ops *fops;
	unsigned long addr;

	/* Cache missing addresses. */
	addr = kgr_get_function_address(patch_fun);
	if (IS_ERR_VALUE(addr))
		return addr;

	pr_debug("kgr: storing %lx to loc_old for %s\n",
			addr, patch_fun->name);
	patch_fun->loc_old = addr;

	/* Initialize ftrace_ops structures for fast and slow stubs. */
	fops = &patch_fun->ftrace_ops_fast;
	fops->private = patch_fun;
	fops->func = kgr_stub_fast;
	fops->flags = FTRACE_OPS_FL_SAVE_REGS;

	fops = &patch_fun->ftrace_ops_slow;
	fops->private = patch_fun;
	fops->func = kgr_stub_slow;
	fops->flags = FTRACE_OPS_FL_SAVE_REGS;

	return 0;
}

static int kgr_patch_code(struct kgr_patch_fun *patch_fun, bool final,
		bool revert)
{
	struct ftrace_ops *new_ops = NULL, *unreg_ops = NULL;
	enum kgr_patch_state next_state;
	int err;

	switch (patch_fun->state) {
	case KGR_PATCH_INIT:
		if (revert || final)
			return -EINVAL;
		err = kgr_init_ftrace_ops(patch_fun);
		if (err) {
			if (err == -ENOENT && !patch_fun->abort_if_missing) {
				patch_fun->state = KGR_PATCH_SKIPPED;
				return 0;
			}
			return err;
		}

		next_state = KGR_PATCH_SLOW;
		new_ops = &patch_fun->ftrace_ops_slow;
		break;
	case KGR_PATCH_SLOW:
		if (revert || !final)
			return -EINVAL;
		next_state = KGR_PATCH_APPLIED;
		new_ops = &patch_fun->ftrace_ops_fast;
		unreg_ops = &patch_fun->ftrace_ops_slow;
		break;
	case KGR_PATCH_APPLIED:
		if (!revert || final)
			return -EINVAL;
		next_state = KGR_PATCH_REVERT_SLOW;
		new_ops = &patch_fun->ftrace_ops_slow;
		unreg_ops = &patch_fun->ftrace_ops_fast;
		break;
	case KGR_PATCH_REVERT_SLOW:
		if (!revert || !final)
			return -EINVAL;
		next_state = KGR_PATCH_REVERTED;
		unreg_ops = &patch_fun->ftrace_ops_slow;
		break;
	case KGR_PATCH_SKIPPED:
		return 0;
	default:
		return -EINVAL;
	}

	/*
	 * In case of error the caller can still have a chance to restore the
	 * previous consistent state.
	 */
	err = kgr_switch_fops(patch_fun, new_ops, unreg_ops);
	if (err)
		return err;

	patch_fun->state = next_state;

	pr_debug("kgr: redirection for %s done\n", patch_fun->name);

	return 0;
}

int kgr_modify_kernel(struct kgr_patch *patch, bool revert)
{
	struct kgr_patch_fun *patch_fun;
	int ret;

	if (!kgr_initialized) {
		pr_err("kgr: can't patch, not initialized\n");
		return -EINVAL;
	}

	mutex_lock(&kgr_in_progress_lock);
	if (kgr_in_progress) {
		pr_err("kgr: can't patch, another patching not yet finalized\n");
		ret = -EAGAIN;
		goto err_unlock;
	}

	pr_info("kgr: %sing patch '%s'\n", revert ? "revert" : "apply",
			patch->name);

	set_bit(0, kgr_immutable);
	wmb(); /* set_bit before kgr_handle_processes */

	kgr_for_each_patch_fun(patch, patch_fun) {
		ret = kgr_patch_code(patch_fun, false, revert);
		/*
		 * In case any of the symbol resolutions in the set
		 * has failed, patch all the previously replaced fentry
		 * callsites back to nops and fail with grace
		 */
		if (ret < 0) {
			for (patch_fun--; patch_fun >= patch->patches;
					patch_fun--)
				if (patch_fun->state == KGR_PATCH_SLOW)
					kgr_ftrace_disable(patch_fun,
						&patch_fun->ftrace_ops_slow);
			goto err_unlock;
		}
	}
	kgr_in_progress = true;
	kgr_patch = patch;
	kgr_revert = revert;
	mutex_unlock(&kgr_in_progress_lock);

	kgr_handle_processes();
	wmb(); /* clear_bit after kgr_handle_processes */
	clear_bit(0, kgr_immutable);

	/*
	 * There is no need to have an explicit barrier here. wake_up_process()
	 * implies a write barrier. That is every woken up task sees
	 * kgr_immutable cleared.
	 */
	kgr_wakeup_kthreads();
	/*
	 * give everyone time to exit kernel, and check after a while
	 */
	queue_delayed_work(kgr_wq, &kgr_work, KGR_TIMEOUT * HZ);

	return 0;
err_unlock:
	mutex_unlock(&kgr_in_progress_lock);

	return ret;
}

/**
 * kgr_patch_kernel -- the entry for a kgraft patch
 * @patch: patch to be applied
 *
 * Start patching of code that is not running in IRQ context.
 */
int kgr_patch_kernel(struct kgr_patch *patch)
{
	int ret;

	if (!try_module_get(patch->owner)) {
		pr_err("kgr: can't increase patch module refcount\n");
		return -EBUSY;
	}

	init_completion(&patch->finish);

	ret = kgr_patch_dir_add(patch);
	if (ret)
		goto err_put;

	ret = kgr_modify_kernel(patch, false);
	if (ret)
		goto err_dir_del;

	return ret;
err_dir_del:
	kgr_patch_dir_del(patch);
err_put:
	module_put(patch->owner);

	return ret;
}
EXPORT_SYMBOL_GPL(kgr_patch_kernel);

/**
 * kgr_patch_remove -- module with this patch is leaving
 *
 * @patch: this patch is going away
 */
void kgr_patch_remove(struct kgr_patch *patch)
{
	kgr_patch_dir_del(patch);
}
EXPORT_SYMBOL_GPL(kgr_patch_remove);

static int __init kgr_init(void)
{
	int ret;

	if (ftrace_is_dead()) {
		pr_warn("kgr: enabled, but ftrace is disabled ... aborting\n");
		return -ENODEV;
	}

	ret = kgr_add_files();
	if (ret)
		return ret;

	/*
	 * This callchain:
	 * kgr_work_fn->kgr_finalize->kgr_patch_code->kgr_switch_fops->
	 *   kgr_ftrace_disable->unregister_ftrace_function->ftrace_shutdown->
	 *   schedule_on_each_cpu->flush_work
	 * triggers a warning that WQ_MEM_RECLAIM is flushing !WQ_MEM_RECLAIM
	 * workqueue. So we have to allocate a !WQ_MEM_RECLAIM workqueue.
	 */
	kgr_wq = alloc_ordered_workqueue("kgraft", 0);
	if (!kgr_wq) {
		pr_err("kgr: cannot allocate a work queue, aborting!\n");
		ret = -ENOMEM;
		goto err_remove_files;
	}

	kgr_initialized = true;
	pr_info("kgr: successfully initialized\n");

	return 0;
err_remove_files:
	kgr_remove_files();

	return ret;
}
module_init(kgr_init);
