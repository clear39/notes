
//	@	drivers/tty/sysrq.c

device_initcall(sysrq_init);

static int __init sysrq_init(void)
{
	sysrq_init_procfs();

	if (sysrq_on())
		sysrq_register_handler();

	return 0;
}

static void sysrq_init_procfs(void)
{
	if (!proc_create("sysrq-trigger", S_IWUSR, NULL, &proc_sysrq_trigger_operations))
		pr_err("Failed to register proc interface\n");
}


static const struct file_operations proc_sysrq_trigger_operations = {
	.write		= write_sysrq_trigger,
	.llseek		= noop_llseek,
};

/*
 * writing 'C' to /proc/sysrq-trigger is like sysrq-C
 */
static ssize_t write_sysrq_trigger(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if (count) {
		char c;

		if (get_user(c, buf))
			return -EFAULT;

		__handle_sysrq(c, false);
	}

	return count;
}


// CONFIG_MAGIC_SYSRQ_DEFAULT_ENABLE=0x1
static int __read_mostly sysrq_enabled = CONFIG_MAGIC_SYSRQ_DEFAULT_ENABLE;

static bool __read_mostly sysrq_always_enabled;

static bool sysrq_on(void)
{
	return sysrq_enabled || sysrq_always_enabled;
}

// @	include/linux/init.h
// __setup 和 early_param 实现差不多的
__setup("sysrq_always_enabled", sysrq_always_enabled_setup);



void __handle_sysrq(int key, bool check_mask)
{
	struct sysrq_key_op *op_p;
	int orig_log_level;
	int i;

	rcu_sysrq_start();
	rcu_read_lock();
	/*
	 * Raise the apparent loglevel to maximum so that the sysrq header
	 * is shown to provide the user with positive feedback.  We do not
	 * simply emit this at KERN_EMERG as that would change message
	 * routing in the consumers of /proc/kmsg.
	 */
	orig_log_level = console_loglevel;
	console_loglevel = CONSOLE_LOGLEVEL_DEFAULT;
	pr_info("SysRq : ");

    op_p = __sysrq_get_key_op(key);
    if (op_p) {
		/*
		 * Should we check for enabled operations (/proc/sysrq-trigger
		 * should not) and is the invoked operation enabled?
		 */
		if (!check_mask || sysrq_on_mask(op_p->enable_mask)) {
			pr_cont("%s\n", op_p->action_msg);
			console_loglevel = orig_log_level;
			op_p->handler(key);
		} else {
			pr_cont("This sysrq operation is disabled.\n");
		}
		
	} else {

		pr_cont("HELP : ");
		/* Only print the help msg once per handler */
		for (i = 0; i < ARRAY_SIZE(sysrq_key_table); i++) {
			if (sysrq_key_table[i]) {
				int j;

				for (j = 0; sysrq_key_table[i] != sysrq_key_table[j]; j++)
					;
				if (j != i)
					continue;

				pr_cont("%s ", sysrq_key_table[i]->help_msg);
			}
		}
		pr_cont("\n");

		console_loglevel = orig_log_level;
	}
	rcu_read_unlock();
	rcu_sysrq_end();
}

/*
 * get and put functions for the table, exposed to modules.
 */
struct sysrq_key_op *__sysrq_get_key_op(int key)
{
    struct sysrq_key_op *op_p = NULL;
    int i;

    // 0~9 都是调用 sysrq_loglevel_op 
    // 
	i = sysrq_key_table_key2index(key);
	if (i != -1)
        op_p = sysrq_key_table[i];

    return op_p;
}

/* key2index calculation, -1 on invalid index */
static int sysrq_key_table_key2index(int key)
{
	int retval;

	if ((key >= '0') && (key <= '9'))
		retval = key - '0';
	else if ((key >= 'a') && (key <= 'z'))
		retval = key + 10 - 'a';
	else
		retval = -1;
	return retval;
}


static struct sysrq_key_op *sysrq_key_table[36] = {
	&sysrq_loglevel_op,		/* 0 */
	&sysrq_loglevel_op,		/* 1 */
	&sysrq_loglevel_op,		/* 2 */
	&sysrq_loglevel_op,		/* 3 */
	&sysrq_loglevel_op,		/* 4 */
	&sysrq_loglevel_op,		/* 5 */
	&sysrq_loglevel_op,		/* 6 */
	&sysrq_loglevel_op,		/* 7 */
	&sysrq_loglevel_op,		/* 8 */
	&sysrq_loglevel_op,		/* 9 */

	/*
	 * a: Don't use for system provided sysrqs, it is handled specially on
	 * sparc and will never arrive.
	 */
	NULL,				/* a */
	&sysrq_reboot_op,		/* b */
	&sysrq_crash_op,		/* c */
	&sysrq_showlocks_op,		/* d */
	&sysrq_term_op,			/* e */
	&sysrq_moom_op,			/* f */
	/* g: May be registered for the kernel debugger */
	NULL,				/* g */
	NULL,				/* h - reserved for help */
	&sysrq_kill_op,			/* i */
#ifdef CONFIG_BLOCK
	&sysrq_thaw_op,			/* j */
#else
	NULL,				/* j */
#endif
	&sysrq_SAK_op,			/* k */
#ifdef CONFIG_SMP
	&sysrq_showallcpus_op,		/* l */
#else
	NULL,				/* l */
#endif
	&sysrq_showmem_op,		/* m */
	&sysrq_unrt_op,			/* n */
	/* o: This will often be registered as 'Off' at init time */
	NULL,				/* o */
	&sysrq_showregs_op,		/* p */
	&sysrq_show_timers_op,		/* q */
	&sysrq_unraw_op,		/* r */
	&sysrq_sync_op,			/* s */
	&sysrq_showstate_op,		/* t */
	&sysrq_mountro_op,		/* u */
	/* v: May be registered for frame buffer console restore */
	NULL,				/* v */
	&sysrq_showstate_blocked_op,	/* w */
	/* x: May be registered on mips for TLB dump */
	/* x: May be registered on ppc/powerpc for xmon */
	/* x: May be registered on sparc64 for global PMU dump */
	NULL,				/* x */
	/* y: May be registered on sparc64 for global register dump */
	NULL,				/* y */
	&sysrq_ftrace_dump_op,		/* z */
};



