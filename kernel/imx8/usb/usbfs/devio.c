

//  @   drivers/usb/core/devio.c
/**
 * subsys_initcall(usb_init) // @ drivers/usb/core/usb.c
 * static int __init usb_init(void)  // @ drivers/usb/core/usb.c
*/
int __init usb_devio_init(void)
{
	int retval;

    /**
	 * #define USB_MAJOR			180 
	 * #define USB_DEVICE_MAJOR		189
     * #define USB_DEVICE_DEV		MKDEV(USB_DEVICE_MAJOR, 0)  // MKDEV 一个2字节，USB_DEVICE_MAJOR为高5位，0为低8位
	 * 
	 * #define USB_MAXBUS			64
	 * #define USB_DEVICE_MAX			(USB_MAXBUS * 128)  // 64*128=
	 * 
    */
	retval = register_chrdev_region(USB_DEVICE_DEV, USB_DEVICE_MAX,"usb_device");
	if (retval) {
		printk(KERN_ERR "Unable to register minors for usb_device\n");
		goto out;
	}
	/**
	 * 
	*/
	cdev_init(&usb_device_cdev, &usbdev_file_operations);
	/**
	 * 
	*/
	retval = cdev_add(&usb_device_cdev, USB_DEVICE_DEV, USB_DEVICE_MAX);
	if (retval) {
		printk(KERN_ERR "Unable to get usb_device major %d\n",USB_DEVICE_MAJOR);
		goto error_cdev;
	}
	/**
	 * 
	*/
	usb_register_notify(&usbdev_nb);
out:
	return retval;

error_cdev:
	/**
	 * 
	*/
	unregister_chrdev_region(USB_DEVICE_DEV, USB_DEVICE_MAX);
	goto out;
}


/**
 * usb_register_notify - register a notifier callback whenever a usb change happens
 * @nb: pointer to the notifier block for the callback events.
 *
 * These changes are either USB devices or busses being added or removed.
 */
void usb_register_notify(struct notifier_block *nb)
{
	/**
	 * drivers/usb/core/notify.c:21:static BLOCKING_NOTIFIER_HEAD(usb_notifier_list);
	*/
	blocking_notifier_chain_register(&usb_notifier_list, nb);
}

#define BLOCKING_INIT_NOTIFIER_HEAD(name) do {  \
        init_rwsem(&(name)->rwsem); \
        (name)->head = NULL;        \
    } while (0

#define init_rwsem(sem)                     \                                                                                                                                                                  
do {                                \
    static struct lock_class_key __key;         \
    __init_rwsem((sem), #sem, &__key);          \
} while (0)


/*
 * initialise the semaphore
 */
void __init_rwsem(struct rw_semaphore *sem, const char *name,
          struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    /*
     * Make sure we are not reinitializing a held semaphore:
     */
    debug_check_no_locks_freed((void *)sem, sizeof(*sem));
    lockdep_init_map(&sem->dep_map, name, key, 0);
#endif
    sem->count = 0;
    raw_spin_lock_init(&sem->wait_lock);
    INIT_LIST_HEAD(&sem->wait_list);
}
EXPORT_SYMBOL(__init_rwsem);



/**
 *  blocking_notifier_chain_register - Add notifier to a blocking notifier chain                                                                                                                               
 *  @nh: Pointer to head of the blocking notifier chain
 *  @n: New entry in notifier chain
 *
 *  Adds a notifier to a blocking notifier chain.
 *  Must be called in process context.
 *
 *  Currently always returns zero.
 */
int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
        struct notifier_block *n)
{
    int ret;

    /*
     * This code gets used during boot-up, when task switching is
     * not yet working and interrupts must remain disabled.  At
     * such times we must not call down_write().
     */
    if (unlikely(system_state == SYSTEM_BOOTING))
        return notifier_chain_register(&nh->head, n);

	/**
	 * kernel/locking/rwsem.c
	*/
    down_write(&nh->rwsem);
    ret = notifier_chain_register(&nh->head, n);
    up_write(&nh->rwsem);
    return ret;
}

/*
 * lock for writing
 */
void __sched down_write(struct rw_semaphore *sem)
{
    might_sleep();
    rwsem_acquire(&sem->dep_map, 0, 0, _RET_IP_);

    LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
    rwsem_set_owner(sem);
}

/*
 *  Notifier chain core routines.  The exported routines below
 *  are layered on top of these, with appropriate locking added.
 */

static int notifier_chain_register(struct notifier_block **nl,                                                                                                                                                 
        struct notifier_block *n)
{
    while ((*nl) != NULL) {
        if (n->priority > (*nl)->priority)
            break;
        nl = &((*nl)->next);
    }
    n->next = *nl;
    rcu_assign_pointer(*nl, n);
    return 0;
}


static int usbdev_notify(struct notifier_block *self,
			       unsigned long action, void *dev)
{
	switch (action) {
	case USB_DEVICE_ADD:
		break;
	case USB_DEVICE_REMOVE:
		usbdev_remove(dev);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block usbdev_nb = {
	.notifier_call =	usbdev_notify,
};