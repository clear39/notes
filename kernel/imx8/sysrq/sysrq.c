
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

// 
__setup("sysrq_always_enabled", sysrq_always_enabled_setup);



static struct input_handler sysrq_handler = {
	.filter		= sysrq_filter,
	.connect	= sysrq_connect,
	.disconnect	= sysrq_disconnect,
	.name		= "sysrq",
	.id_table	= sysrq_ids,
};


static inline void sysrq_register_handler(void)
{
	int error;

	sysrq_of_get_keyreset_config();

	error = input_register_handler(&sysrq_handler);
	if (error)
		pr_err("Failed to register input handler, error %d", error);
	else
		sysrq_handler_registered = true;
}


static void sysrq_of_get_keyreset_config(void)
{
	u32 key;
	struct device_node *np;
	struct property *prop;
	const __be32 *p;

	np = of_find_node_by_path("/chosen/linux,sysrq-reset-seq");
	if (!np) {
		pr_debug("No sysrq node found");
		return;
	}

	/* Reset in case a __weak definition was present */
	sysrq_reset_seq_len = 0;

	of_property_for_each_u32(np, "keyset", prop, p, key) {
		if (key == KEY_RESERVED || key > KEY_MAX ||
		    sysrq_reset_seq_len == SYSRQ_KEY_RESET_MAX)
			break;

		sysrq_reset_seq[sysrq_reset_seq_len++] = (unsigned short)key;
	}

	/* Get reset timeout if any. */
	of_property_read_u32(np, "timeout-ms", &sysrq_reset_downtime_ms);
}



//	@	drivers/input/input.c
int input_register_handler(struct input_handler *handler)
{
	struct input_dev *dev;
	int error;

	error = mutex_lock_interruptible(&input_mutex);
	if (error)
		return error;

	INIT_LIST_HEAD(&handler->h_list);

	// 将handler->node添加input_handler_list链表尾端
	// drivers/input/input.c:41:static LIST_HEAD(input_handler_list);
	list_add_tail(&handler->node, &input_handler_list);

	// static LIST_HEAD(input_dev_list);
	list_for_each_entry(dev, &input_dev_list, node)
		input_attach_handler(dev, handler);

	input_wakeup_procfs_readers();

	mutex_unlock(&input_mutex);

	return 0;
}



static inline void input_wakeup_procfs_readers(void)
{
    input_devices_state++;
    wake_up(&input_devices_poll_wait);
}



/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_next_entry(pos, member))


static int input_attach_handler(struct input_dev *dev, struct input_handler *handler)
{
	const struct input_device_id *id;
	int error;

	id = input_match_device(handler, dev);
	if (!id)
		return -ENODEV;

	error = handler->connect(handler, dev, id);
	if (error && error != -ENODEV)
		pr_err("failed to attach handler %s to device %s, error: %d\n", handler->name, kobject_name(&dev->dev.kobj), error);

	return error;
}


static const struct input_device_id *input_match_device(struct input_handler *handler, struct input_dev *dev)
{
	const struct input_device_id *id;

	for (id = handler->id_table; id->flags || id->driver_info; id++) {
		if (input_match_device_id(dev, id) &&
		    (!handler->match || handler->match(handler, dev))) {
			return id;
		}
	}

	return NULL;
}




tatic int sysrq_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id)
{
	struct sysrq_state *sysrq;
	int error;

	sysrq = kzalloc(sizeof(struct sysrq_state), GFP_KERNEL);
	if (!sysrq)
		return -ENOMEM;

	INIT_WORK(&sysrq->reinject_work, sysrq_reinject_alt_sysrq);

	sysrq->handle.dev = dev;
	sysrq->handle.handler = handler;
	sysrq->handle.name = "sysrq";
	sysrq->handle.private = sysrq;
	setup_timer(&sysrq->keyreset_timer, sysrq_do_reset, (unsigned long)sysrq);

	error = input_register_handle(&sysrq->handle);
	if (error) {
		pr_err("Failed to register input sysrq handler, error %d\n", error);
		goto err_free;
	}

	error = input_open_device(&sysrq->handle);
	if (error) {
		pr_err("Failed to open input device, error %d\n", error);
		goto err_unregister;
	}

	return 0;

 err_unregister:
	input_unregister_handle(&sysrq->handle);
 err_free:
	kfree(sysrq);
	return error;
}
