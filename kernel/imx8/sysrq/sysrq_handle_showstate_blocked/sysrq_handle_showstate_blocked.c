

// @	drivers/tty/sysrq.c

static struct sysrq_key_op sysrq_showstate_blocked_op = {
	.handler	= sysrq_handle_showstate_blocked,
	.help_msg	= "show-blocked-tasks(w)",
	.action_msg	= "Show Blocked State",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
}

static void sysrq_handle_showstate_blocked(int key)
{
	// #define TASK_UNINTERRUPTIBLE	2
	show_state_filter(TASK_UNINTERRUPTIBLE);
}


//	@	kernel/sched/core.c
void show_state_filter(unsigned long state_filter)
{
	struct task_struct *g, *p;

#if BITS_PER_LONG == 32
	printk(KERN_INFO "  task                PC stack   pid father\n");
#else
	printk(KERN_INFO "  task                PC stack   pid father\n");
#endif

	rcu_read_lock();
	// @ include/linux/sched/signal.h
	for_each_process_thread(g, p) {
		/*
		 * reset the NMI-timeout, listing all files on a slow
		 * console might take a lot of time:
		 * Also, reset softlockup watchdogs on all CPUs, because
		 * another CPU might be blocked waiting for us to process
		 * an IPI.
		 */
		touch_nmi_watchdog();
		touch_all_softlockup_watchdogs();
		// kernel/sched/core.c
		if (state_filter_match(state_filter, p))
			sched_show_task(p);
	}

#ifdef CONFIG_SCHED_DEBUG  //CONFIG_SCHED_DEBUG=y
	if (!state_filter)
		sysrq_sched_debug_show();
#endif
	rcu_read_unlock();
	/*
	 * Only show locks if all tasks are dumped:
	 */
	if (!state_filter)
		debug_show_all_locks();
}



#define for_each_process_thread(p, t)	\
	for_each_process(p) 				\
		for_each_thread(p, t)

for (p = &init_task ; (p = list_entry_rcu((p)->tasks.next, struct task_struct, tasks)) != &init_task ; )
		list_for_each_entry_rcu(t, &(signal)->thread_head, thread_node)

#define for_each_process(p) \
	for (p = &init_task ; (p = next_task(p)) != &init_task ; )


#define next_task(p) \
	list_entry_rcu((p)->tasks.next, struct task_struct, tasks)

#define for_each_thread(p, t)		\
	__for_each_thread((p)->signal, t)

#define __for_each_thread(signal, t)	\
	list_for_each_entry_rcu(t, &(signal)->thread_head, thread_node)





static inline bool
state_filter_match(unsigned long state_filter, struct task_struct *p)
{
	/* no filter, everything matches */
	if (!state_filter)
		return true;

	/* filter, but doesn't match */
	if (!(p->state & state_filter))
		return false;

	/*
	 * When looking for TASK_UNINTERRUPTIBLE skip TASK_IDLE (allows
	 * TASK_KILLABLE).
	 */
	if (state_filter == TASK_UNINTERRUPTIBLE && p->state == TASK_IDLE)
		return false;

	return true;
}



void sched_show_task(struct task_struct *p)
{
	unsigned long free = 0;
	int ppid;

	if (!try_get_task_stack(p))
		return;

	printk(KERN_INFO "%-15.15s %c", p->comm, task_state_to_char(p));

	if (p->state == TASK_RUNNING)
		printk(KERN_CONT "  running task    ");


#ifdef CONFIG_DEBUG_STACK_USAGE //没有定义
	free = stack_not_used(p);
#endif

	ppid = 0;
	rcu_read_lock();
	if (pid_alive(p))
		ppid = task_pid_nr(rcu_dereference(p->real_parent));

	rcu_read_unlock();

	printk(KERN_CONT "%5lu %5d %6d 0x%08lx\n", free,
		task_pid_nr(p), ppid,
		(unsigned long)task_thread_info(p)->flags);

	print_worker_info(KERN_INFO, p);

	show_stack(p, NULL);

	put_task_stack(p);
}



/**
 * print_worker_info - print out worker information and description
 * @log_lvl: the log level to use when printing
 * @task: target task
 *
 * If @task is a worker and currently executing a work item, print out the
 * name of the workqueue being serviced and worker description set with
 * set_worker_desc() by the currently executing work item.
 *
 * This function can be safely called on any task as long as the
 * task_struct itself is accessible.  While safe, this function isn't
 * synchronized and may print out mixups or garbages of limited length.
 */
void print_worker_info(const char *log_lvl, struct task_struct *task)
{
	work_func_t *fn = NULL;
	char name[WQ_NAME_LEN] = { };
	char desc[WORKER_DESC_LEN] = { };
	struct pool_workqueue *pwq = NULL;
	struct workqueue_struct *wq = NULL;
	bool desc_valid = false;
	struct worker *worker;

	if (!(task->flags & PF_WQ_WORKER))
		return;

	/*
	 * This function is called without any synchronization and @task
	 * could be in any state.  Be careful with dereferences.
	 */
	worker = kthread_probe_data(task);

	/*
	 * Carefully copy the associated workqueue's workfn and name.  Keep
	 * the original last '\0' in case the original contains garbage.
	 */
	probe_kernel_read(&fn, &worker->current_func, sizeof(fn));
	probe_kernel_read(&pwq, &worker->current_pwq, sizeof(pwq));
	probe_kernel_read(&wq, &pwq->wq, sizeof(wq));
	probe_kernel_read(name, wq->name, sizeof(name) - 1);

	/* copy worker description */
	probe_kernel_read(&desc_valid, &worker->desc_valid, sizeof(desc_valid));

	if (desc_valid)
		probe_kernel_read(desc, worker->desc, sizeof(desc) - 1);

	if (fn || name[0] || desc[0]) {

		printk("%sWorkqueue: %s %pf", log_lvl, name, fn);

		if (desc[0])
			pr_cont(" (%s)", desc);

		pr_cont("\n");
	}
}





void show_stack(struct task_struct *tsk, unsigned long *sp)
{
	dump_backtrace(NULL, tsk);
	barrier();
}



void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
	struct stackframe frame;
	int skip;

	pr_debug("%s(regs = %p tsk = %p)\n", __func__, regs, tsk);

	if (!tsk)
		tsk = current;

	if (!try_get_task_stack(tsk))
		return;

	if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.pc = (unsigned long)dump_backtrace;
	} else {
		/*
		 * task blocked in __switch_to
		 */
		frame.fp = thread_saved_fp(tsk);
		frame.pc = thread_saved_pc(tsk);
	}
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = tsk->curr_ret_stack;
#endif

	skip = !!regs;
	printk("Call trace:\n");
	while (1) {
		unsigned long stack;
		int ret;

		/* skip until specified stack frame */
		if (!skip) {
			dump_backtrace_entry(frame.pc);
		} else if (frame.fp == regs->regs[29]) {
			skip = 0;
			/*
			 * Mostly, this is the case where this function is
			 * called in panic/abort. As exception handler's
			 * stack frame does not contain the corresponding pc
			 * at which an exception has taken place, use regs->pc
			 * instead.
			 */
			dump_backtrace_entry(regs->pc);
		}
		ret = unwind_frame(tsk, &frame);
		if (ret < 0)
			break;
		if (in_entry_text(frame.pc)) {
			stack = frame.fp - offsetof(struct pt_regs, stackframe);

			if (on_accessible_stack(tsk, stack))
				dump_mem("", "Exception stack", stack, stack + sizeof(struct pt_regs));
		}
	}

	put_task_stack(tsk);
}

void show_stack(struct task_struct *tsk, unsigned long *sp)
{
	dump_backtrace(NULL, tsk);
	barrier();
}


void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
	struct stackframe frame;
	int skip;

	pr_debug("%s(regs = %p tsk = %p)\n", __func__, regs, tsk);

	if (!tsk)
		tsk = current;

	if (!try_get_task_stack(tsk))
		return;

	if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.pc = (unsigned long)dump_backtrace;
	} else {
		/*
		 * task blocked in __switch_to
		 */
		frame.fp = thread_saved_fp(tsk);
		frame.pc = thread_saved_pc(tsk);
	}
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = tsk->curr_ret_stack;
#endif

	skip = !!regs;
	printk("Call trace:\n");
	while (1) {
		unsigned long stack;
		int ret;

		/* skip until specified stack frame */
		if (!skip) {
			dump_backtrace_entry(frame.pc);
		} else if (frame.fp == regs->regs[29]) {
			skip = 0;
			/*
			 * Mostly, this is the case where this function is
			 * called in panic/abort. As exception handler's
			 * stack frame does not contain the corresponding pc
			 * at which an exception has taken place, use regs->pc
			 * instead.
			 */
			dump_backtrace_entry(regs->pc);
		}

		ret = unwind_frame(tsk, &frame);
		if (ret < 0)
			break;
		if (in_entry_text(frame.pc)) {
			stack = frame.fp - offsetof(struct pt_regs, stackframe);

			if (on_accessible_stack(tsk, stack))
				dump_mem("", "Exception stack", stack,
					 stack + sizeof(struct pt_regs));
		}

	}

	put_task_stack(tsk);
}