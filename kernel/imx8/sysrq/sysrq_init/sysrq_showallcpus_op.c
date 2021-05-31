
static struct sysrq_key_op sysrq_showallcpus_op = {
	.handler	= sysrq_handle_showallcpus,
	.help_msg	= "show-backtrace-all-active-cpus(l)",
	.action_msg	= "Show backtrace of all active CPUs",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};

static void sysrq_handle_showallcpus(int key)
{
	/*
	 * Fall back to the workqueue based printing if the
	 * backtrace printing did not succeed or the
	 * architecture has no support for it:
	 */
	if (!trigger_all_cpu_backtrace()) {
		struct pt_regs *regs = NULL;

		if (in_irq())
			regs = get_irq_regs();

		if (regs) {
			pr_info("CPU%d:\n", smp_processor_id());
			show_regs(regs);
		}

		schedule_work(&sysrq_showallcpus);
	}
}

static struct sysrq_key_op sysrq_showallcpus_op = {
	.handler	= sysrq_handle_showallcpus,
	.help_msg	= "show-backtrace-all-active-cpus(l)",
	.action_msg	= "Show backtrace of all active CPUs",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};

static DECLARE_WORK(sysrq_showallcpus, sysrq_showregs_othercpus);

static void sysrq_handle_showallcpus(int key)
{
	/*
	 * Fall back to the workqueue based printing if the
	 * backtrace printing did not succeed or the
	 * architecture has no support for it:
	 */
	if (!trigger_all_cpu_backtrace()) {
		struct pt_regs *regs = NULL;

		if (in_irq())
			regs = get_irq_regs();

		if (regs) {
			pr_info("CPU%d:\n", smp_processor_id());
			show_regs(regs);
		}
		
		schedule_work(&sysrq_showallcpus);
	}
}

