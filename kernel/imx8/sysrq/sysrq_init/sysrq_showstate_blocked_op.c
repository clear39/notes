

//	@	drivers/tty/sysrq.c


static struct sysrq_key_op sysrq_showstate_blocked_op = {
	.handler	= sysrq_handle_showstate_blocked,
	.help_msg	= "show-blocked-tasks(w)",
	.action_msg	= "Show Blocked State",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};



static void sysrq_handle_showstate_blocked(int key)
{	
	//	TASK_UNINTERRUPTIBLE = 2
	// @	kernel/sched/core.c
	show_state_filter(TASK_UNINTERRUPTIBLE);
}


void show_state_filter(unsigned long state_filter)
{
	struct task_struct *g, *p;

#if BITS_PER_LONG == 32
	printk(KERN_INFO "  task                PC stack   pid father\n");
#else
	printk(KERN_INFO "  task                        PC stack   pid father\n");
#endif
	rcu_read_lock();
	
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
		if (state_filter_match(state_filter, p))
			sched_show_task(p);
	}

#ifdef CONFIG_SCHED_DEBUG
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