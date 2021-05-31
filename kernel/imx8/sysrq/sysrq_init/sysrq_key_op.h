


struct sysrq_key_op {
	void (*handler)(int);
	char *help_msg;
	char *action_msg;
	int enable_mask;
};
