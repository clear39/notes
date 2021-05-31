


static void sysrq_handle_loglevel(int key)
{
	int i;

	i = key - '0';
	console_loglevel = CONSOLE_LOGLEVEL_DEFAULT;
	pr_info("Loglevel set to %d\n", i);
	console_loglevel = i;
}


static struct sysrq_key_op sysrq_loglevel_op = {
	.handler	= sysrq_handle_loglevel,
	.help_msg	= "loglevel(0-9)",
	.action_msg	= "Changing Loglevel",
	.enable_mask	= SYSRQ_ENABLE_LOG,
};